// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "core_v2/internal/bwu_manager.h"

#include <algorithm>

#include "core_v2/internal/bwu_handler.h"
#include "core_v2/internal/offline_frames.h"
#include "core_v2/internal/webrtc_bwu_handler.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/public/count_down_latch.h"
#include "proto/connections_enums.pb.h"
#include "absl/functional/bind_front.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace connections {

using ::location::nearby::proto::connections::ConnectionAttemptResult;
using ::location::nearby::proto::connections::DisconnectionReason;

BwuManager::BwuManager(
    Mediums& mediums, EndpointManager& endpoint_manager,
    EndpointChannelManager& channel_manager,
    absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers,
    Config config)
    : config_(config),
      mediums_(&mediums),
      endpoint_manager_(&endpoint_manager),
      channel_manager_(&channel_manager) {
  if (config_.bandwidth_upgrade_retry_delay == absl::ZeroDuration()) {
    config_.bandwidth_upgrade_retry_delay = absl::Seconds(5);
  }
  if (config_.bandwidth_upgrade_retry_delay == absl::ZeroDuration()) {
    config_.bandwidth_upgrade_retry_delay = absl::Seconds(10);
  }
  if (config_.allow_upgrade_to.All(false)) {
    config.allow_upgrade_to.web_rtc = true;
  }
  if (!handlers.empty()) {
    handlers_ = std::move(handlers);
  } else {
    InitBwuHandlers();
  }

  // Register the offline frame processor.
  endpoint_manager.RegisterFrameProcessor(
      V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION, this);
}

void BwuManager::InitBwuHandlers() {
  // Register the supported concrete BwuMedium implementations.
  BwuHandler::BwuNotifications notifications{
      .incoming_connection_cb =
          absl::bind_front(&BwuManager::OnIncomingConnection, this),
  };
  if (config_.allow_upgrade_to.web_rtc) {
    handlers_.emplace(Medium::WEB_RTC,
                      std::make_unique<WebrtcBwuHandler>(
                          *mediums_, *channel_manager_, notifications));
  }
}

void BwuManager::Shutdown() {
  NEARBY_LOG(INFO, "Initiating shutdown of BwuManager.");

  endpoint_manager_->UnregisterFrameProcessor(
      V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION, this);

  CountDownLatch latch(1);

  RunOnBwuManagerThread([this, &latch]() {
    for (auto& item : previous_endpoint_channels_) {
      EndpointChannel* channel = item.second.get();
      if (!channel) continue;
      channel->Close(DisconnectionReason::SHUTDOWN);
    }

    CancelAllRetryUpgradeAlarms();
    medium_ = Medium::UNKNOWN_MEDIUM;
    for (auto& item : handlers_) {
      BwuHandler& handler = *item.second;
      handler.Revert();
    }
    handlers_.clear();
    latch.CountDown();
  });

  latch.Await();

  // Stop all the ongoing Runnables (as gracefully as possible).
  alarm_executor_.Shutdown();
  serial_executor_.Shutdown();

  NEARBY_LOG(INFO, "BwuHandler has shut down.");
}

// This is the point on the Initiator side where the
// medium_ is set.
void BwuManager::InitiateBwuForEndpoint(ClientProxy* client,
                                        const std::string& endpoint_id,
                                        Medium new_medium) {
  RunOnBwuManagerThread([this, client, endpoint_id, new_medium]() {
    Medium proposed_medium = ChooseBestUpgradeMedium(
        client->GetUpgradeMediums(endpoint_id).GetMediums(true));
    if (new_medium != Medium::UNKNOWN_MEDIUM) {
      proposed_medium = new_medium;
    }
    auto* handler = SetCurrentBwuHandler(proposed_medium);

    if (!handler) return;

    if (in_progress_upgrades_.contains(endpoint_id)) {
      return;
    }

    auto channel = channel_manager_->GetChannelForEndpoint(endpoint_id);

    if (channel == nullptr) {
      return;
    }

    // Ignore requests where the medium we're upgrading to is the medium we're
    // already connected over. This can happen now that Bluetooth is both an
    // advertising medium and a potential bandwidth upgrade, and will continue
    // to be possible as we add other new advertising mediums like mDNS (WiFi
    // LAN). Very specifically, this happens now when a device uses P2P_CLUSTER,
    // connects over Bluetooth, and is not connected to LAN. Bluetooth is the
    // best medium, and we attempt to upgrade from Bluetooth to Bluetooth.
    if (medium_ == channel->GetMedium()) {
      return;
    }

    std::string service_id = client->GetServiceId();
    ByteArray bytes = handler->InitializeUpgradedMediumForEndpoint(
        client, service_id, endpoint_id);

    // Because we grab the endpointChannel first thing, it is possible the
    // endpointChannel is stale by the time we attempt to write over it.
    if (bytes.Empty()) {
      NEARBY_LOG(ERROR,
                 "Couldn't complete the upgrade for endpoint "
                 "%s to %d because it failed to initialize the "
                 "BWU_NEGOTIATION.UPGRADE_PATH_AVAILABLE OfflineFrame.",
                 endpoint_id.c_str(), medium_);
      UpgradePathInfo info;
      info.set_medium(parser::MediumToUpgradePathInfoMedium(medium_));

      ProcessUpgradeFailureEvent(client, endpoint_id, info);
      return;
    }
    if (!channel->Write(bytes).Ok()) {
      NEARBY_LOG(ERROR,
                 "Couldn't complete the upgrade for endpoint %s to %d because "
                 "it failed to write the "
                 "BWU_NEGOTIATION.UPGRADE_PATH_AVAILABLE OfflineFrame.",
                 endpoint_id.c_str(), medium_);
      return;
    }

    NEARBY_LOG(INFO,
               "Successfully wrote the BWU_NEGOTIATION.UPGRADE_PATH_AVAILABLE "
               "OfflineFrame while upgrading endpoint %s to %d.",
               endpoint_id.c_str(), medium_);
    in_progress_upgrades_.emplace(endpoint_id, client);
  });
}

void BwuManager::OnIncomingFrame(OfflineFrame& frame,
                                 const std::string& endpoint_id,
                                 ClientProxy* client, Medium medium) {
  if (parser::GetFrameType(frame) != V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION)
    return;
  auto bwu_frame = frame.v1().bandwidth_upgrade_negotiation();
  CountDownLatch latch(1);
  RunOnBwuManagerThread([this, client, endpoint_id, &bwu_frame, &latch]() {
    OnBwuNegotiationFrame(client, bwu_frame, endpoint_id);
    latch.CountDown();
  });
  latch.Await();
}

void BwuManager::OnEndpointDisconnect(ClientProxy* client,
                                      const std::string& endpoint_id,
                                      CountDownLatch* barrier) {
  RunOnBwuManagerThread([this, client, endpoint_id, barrier]() {
    if (medium_ == Medium::UNKNOWN_MEDIUM) {
      barrier->CountDown();
      return;
    }

    if (handler_) {
      handler_->OnEndpointDisconnect(client, endpoint_id);
    }

    auto item = old_channels_.extract(endpoint_id);

    if (!item.empty()) {
      auto old_channel = item.mapped();
      if (old_channel != nullptr) {
        old_channel->Close(DisconnectionReason::SHUTDOWN);
      }
    }
    in_progress_upgrades_.erase(endpoint_id);
    CancelRetryUpgradeAlarm(endpoint_id);

    successfully_upgraded_endpoints_.erase(endpoint_id);

    // If this was our very last endpoint:
    //
    // a) revert all the changes for currentBwuMedium.
    // b) reset currentBwuMedium.
    if (channel_manager_->GetConnectedEndpointsCount() <= 1) {
      Revert();
    }
    barrier->CountDown();
  });
}

BwuHandler* BwuManager::SetCurrentBwuHandler(Medium medium) {
  handler_ = nullptr;
  medium_ = medium;
  if (medium != Medium::UNKNOWN_MEDIUM) {
    auto item = handlers_.find(medium);
    if (item != handlers_.end()) {
      handler_ = item->second.get();
    }
  }
  return handler_;
}

void BwuManager::Revert() {
  if (handler_) {
    handler_->Revert();
    medium_ = Medium::UNKNOWN_MEDIUM;
    handler_ = nullptr;
  }
}

void BwuManager::OnBwuNegotiationFrame(ClientProxy* client,
                                       const BwuNegotiationFrame& frame,
                                       const string& endpoint_id) {
  switch (frame.event_type()) {
    case BwuNegotiationFrame::UPGRADE_PATH_AVAILABLE:
      ProcessBwuPathAvailableEvent(client, endpoint_id,
                                   frame.upgrade_path_info());
      break;
    case BwuNegotiationFrame::UPGRADE_FAILURE:
      ProcessUpgradeFailureEvent(client, endpoint_id,
                                 frame.upgrade_path_info());
      break;
    case BwuNegotiationFrame::LAST_WRITE_TO_PRIOR_CHANNEL:
      ProcessLastWriteToPriorChannelEvent(client, endpoint_id);
      break;
    case BwuNegotiationFrame::SAFE_TO_CLOSE_PRIOR_CHANNEL:
      ProcessSafeToClosePriorChannelEvent(client, endpoint_id);
      break;
    default:
      break;
  }
}

void BwuManager::OnIncomingConnection(
    ClientProxy* client, BwuHandler::IncomingSocketConnection* connection) {
  RunOnBwuManagerThread([this, client, connection]() {
    EndpointChannel* channel = connection->channel.get();
    if (channel == nullptr) {
      connection->socket->Close();
      return;
    }

    ClientIntroduction introduction;
    if (!ReadClientIntroductionFrame(channel, introduction)) {
      // This was never a fully EstablishedConnection, no need to provide a
      // closure reason.
      channel->Close();
      return;
    }

    const std::string& endpoint_id = introduction.endpoint_id();
    auto item = in_progress_upgrades_.extract(endpoint_id);
    if (item.empty()) return;
    ClientProxy* mapped_client = item.mapped();
    CancelRetryUpgradeAlarm(endpoint_id);
    if (mapped_client == nullptr) {
      // This was never a fully EstablishedConnection, no need to provide a
      // closure reason.
      channel->Close();
      return;
    }

    CHECK(client == mapped_client);

    // Use the introductory client information sent over to run the upgrade
    // protocol.
    RunUpgradeProtocol(mapped_client, endpoint_id,
                       std::move(connection->channel));
  });
}

void BwuManager::RunOnBwuManagerThread(Runnable runnable) {
  serial_executor_.Execute(std::move(runnable));
}

void BwuManager::RunUpgradeProtocol(
    ClientProxy* client, const std::string& endpoint_id,
    std::unique_ptr<EndpointChannel> new_channel) {
  // First, register this new EndpointChannel as *the* EndpointChannel to use
  // for this endpoint here onwards. NOTE: We pause this new EndpointChannel
  // until we've completely drained the old EndpointChannel to avoid out of
  // order reads on the other side. This is a consequence of using the same
  // UKEY2 context for both the previous and new EndpointChannels. UKEY2 uses
  // sequence numbers for writes and reads, and simultaneously sending Payloads
  // on the new channel and control messages on the old channel cause the other
  // side to read messages out of sequence
  new_channel->Pause();
  auto old_channel = channel_manager_->GetChannelForEndpoint(endpoint_id);
  if (!old_channel) return;
  channel_manager_->ReplaceChannelForEndpoint(client, endpoint_id,
                                              std::move(new_channel));

  // Next, initiate a clean shutdown for the previous EndpointChannel used for
  // this endpoint by telling the remote device that it will not receive any
  // more writes over that EndpointChannel.
  if (!old_channel->Write(parser::ForBwuLastWrite()).Ok()) {
    return;
  }

  // The remainder of this clean shutdown for the previous EndpointChannel will
  // continue when we receive a corresponding
  // BANDWIDTH_UPGRADE_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL OfflineFrame from
  // the remote device, so for now, just store that previous EndpointChannel.
  old_channels_.emplace(endpoint_id, old_channel);

  // If we already read LAST_WRITE on the old endpoint channel, then we can
  // safely close it now.
  auto item = successfully_upgraded_endpoints_.extract(endpoint_id);
  if (!item.empty()) {
    ProcessLastWriteToPriorChannelEvent(client, endpoint_id);
  }
}

// Outgoing BWU session.
void BwuManager::ProcessBwuPathAvailableEvent(
    ClientProxy* client, const string& endpoint_id,
    const UpgradePathInfo& upgrade_path_info) {
  Medium medium =
      parser::UpgradePathInfoMediumToMedium(upgrade_path_info.medium());
  if (medium_ == Medium::UNKNOWN_MEDIUM) {
    SetCurrentBwuHandler(medium);
  }
  // Check for the correct medium so we don't process an incorrect OfflineFrame.
  if (medium != medium_) {
    RunUpgradeFailedProtocol(client, endpoint_id, upgrade_path_info);
    return;
  }

  auto channel = ProcessBwuPathAvailableEventInternal(client, endpoint_id,
                                                      upgrade_path_info);
  ConnectionAttemptResult connectionAttemptResult;
  if (channel != nullptr) {
    connectionAttemptResult = ConnectionAttemptResult::RESULT_SUCCESS;
  } else {
    connectionAttemptResult = ConnectionAttemptResult::RESULT_ERROR;
  }

  if (channel == nullptr) {
    RunUpgradeFailedProtocol(client, endpoint_id, upgrade_path_info);
    return;
  }

  RunUpgradeProtocol(client, endpoint_id, std::move(channel));
}

std::unique_ptr<EndpointChannel>
BwuManager::ProcessBwuPathAvailableEventInternal(
    ClientProxy* client, const string& endpoint_id,
    const UpgradePathInfo& upgrade_path_info) {
  std::unique_ptr<EndpointChannel> channel =
      handler_->CreateUpgradedEndpointChannel(client, client->GetServiceId(),
                                              endpoint_id, upgrade_path_info);
  if (!channel) {
    return nullptr;
  }

  // Write the requisite BANDWIDTH_UPGRADE_NEGOTIATION.CLIENT_INTRODUCTION as
  // the first OfflineFrame on this new EndpointChannel.
  if (!channel->Write(parser::ForBwuIntroduction(client->GetLocalEndpointId()))
           .Ok()) {
    // This was never a fully EstablishedConnection, no need to provide a
    // closure reason.
    channel->Close();

    NEARBY_LOG(
        ERROR,
        "Failed to write BWU_NEGOTIATION.CLIENT_INTRODUCTION OfflineFrame to "
        "newly-created EndpointChannel %s, aborting upgrade.",
        channel->GetName().c_str());

    return {};
  }

  NEARBY_LOG(
      INFO,
      "Successfully wrote  BWU_NEGOTIATION.CLIENT_INTRODUCTION OfflineFrame to "
      "newly-created EndpointChannel %s while upgrading endpoint %s.",
      channel->GetName().c_str(), endpoint_id.c_str());

  // Set the AnalyticsRecorder so that the future closure of this
  // EndpointChannel will be recorded.
  return channel;
}

void BwuManager::RunUpgradeFailedProtocol(
    ClientProxy* client, const std::string& endpoint_id,
    const UpgradePathInfo& upgrade_path_info) {
  // We attempted to connect to the new medium that the remote device has set up
  // for us but we failed. We need to let the remote device know so that they
  // can pick another medium for us to try.
  std::shared_ptr<EndpointChannel> channel =
      channel_manager_->GetChannelForEndpoint(endpoint_id);
  if (!channel) {
    NEARBY_LOG(ERROR,
               "Couldn't find a previous EndpointChannel for %s "
               "when sending an upgrade failure frame, short-circuiting the "
               "upgrade protocol.",
               endpoint_id.c_str());
    return;
  }

  // Report UPGRADE_FAILURE to the remote device.
  if (!channel->Write(parser::ForBwuFailure(upgrade_path_info)).Ok()) {
    channel->Close(DisconnectionReason::IO_ERROR);

    NEARBY_LOG(
        ERROR,
        "Failed to write BANDWIDTH_UPGRADE_NEGOTIATION.UPGRADE_FAILURE "
        "OfflineFrame to endpoint %s, short-circuiting the upgrade protocol.",
        endpoint_id.c_str());
    return;
  }

  // And lastly, clean up our currentBwuMedium since we failed to
  // utilize it anyways.
  if (medium_ != Medium::UNKNOWN_MEDIUM) {
    Revert();
  }
}

bool BwuManager::ReadClientIntroductionFrame(EndpointChannel* channel,
                                             ClientIntroduction& introduction) {
  auto data = channel->Read();
  if (!data.ok()) return false;
  auto transfer(parser::FromBytes(data.result()));
  if (!transfer.ok()) return false;
  OfflineFrame frame = transfer.result();
  if (!frame.has_v1() || !frame.v1().has_bandwidth_upgrade_negotiation())
    return false;
  const auto& frame_intro =
      frame.v1().bandwidth_upgrade_negotiation().client_introduction();
  introduction = frame_intro;
  return true;
}

void BwuManager::ProcessLastWriteToPriorChannelEvent(
    ClientProxy* client, const std::string& endpoint_id) {
  // By this point in the upgrade protocol, there is the guarantee that both
  // involved endpoints have registered a new EndpointChannel with the
  // EndpointChannelManager as the official channel for communication; given
  // the way communication is structured in the EndpointManager, this means
  // that all new writes are happening over that new EndpointChannel, but
  // reads are still happening over this prior EndpointChannel (to avoid data
  // loss). But now that we've received this definitive final write over that
  // prior EndpointChannel, we can let the remote device that they can safely
  // close their end of this now-dormant EndpointChannel.
  EndpointChannel* previous_endpoint_channel =
      previous_endpoint_channels_[endpoint_id].get();
  if (!previous_endpoint_channel) {
    NEARBY_LOG(
        ERROR,
        "Received a BWU_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL OfflineFrame "
        "for unknown endpoint %s, can't complete the upgrade protocol.",
        endpoint_id.c_str());

    successfully_upgraded_endpoints_.emplace(endpoint_id);
    return;
  }

  if (!previous_endpoint_channel->Write(parser::ForBwuSafeToClose()).Ok()) {
    previous_endpoint_channel->Close(DisconnectionReason::IO_ERROR);
    // Remove this prior EndpointChannel from previous_endpoint_channels to
    // avoid leaks.
    previous_endpoint_channels_.erase(endpoint_id);

    NEARBY_LOG(
        ERROR,
        "Failed to write BWU_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL "
        "OfflineFrame to endpoint %s, short-circuiting the upgrade protocol.",
        endpoint_id.c_str());
    return;
  }

  // The upgrade protocol's clean shutdown of the prior EndpointChannel will
  // conclude when we receive a corresponding
  // BANDWIDTH_UPGRADE_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL OfflineFrame
  // from the remote device.
}

void BwuManager::ProcessSafeToClosePriorChannelEvent(
    ClientProxy* client, const std::string& endpoint_id) {
  // By this point in the upgrade protocol, there's no more writes happening
  // over the prior EndpointChannel, and the remote device has given us the
  // go-ahead to close this EndpointChannel [1], so we can safely close it
  // (and depend on the EndpointManager querying the EndpointChannelManager to
  // start reading from the new EndpointChannel).
  //
  // [1] Which also implies that they've received our
  // BANDWIDTH_UPGRADE_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL OfflineFrame),
  // so there can be no data loss, regardless of whether the EndpointChannel
  // allows reads of queued, unread data after the EndpointChannel has been
  // closed from the other end (as is the case with conventional TCP sockets)
  // or not (as is the case with Android's Bluetooth sockets, where closing
  // instantly throws an IOException on the remote device).
  auto item = previous_endpoint_channels_.extract(endpoint_id);
  auto& previous_endpoint_channel = item.mapped();
  if (previous_endpoint_channel == nullptr) {
    NEARBY_LOG(
        ERROR,
        "Received a BWU_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL OfflineFrame "
        "for unknown endpoint %s, can't complete the upgrade protocol.",
        endpoint_id.c_str());
    return;
  }

  NEARBY_LOG(INFO,
             "BwuManager successfully received a "
             "BWU_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL OfflineFrame while "
             "trying to upgrade endpoint %s.",
             endpoint_id.c_str());

  // Wait for in-flight messages to reach their peers.
  SystemClock::Sleep(absl::Seconds(1));
  previous_endpoint_channel->Close(DisconnectionReason::UPGRADED);

  // Now that the old channel has been drained, we can unpause the new channel
  std::shared_ptr<EndpointChannel> channel =
      channel_manager_->GetChannelForEndpoint(endpoint_id);

  if (!channel) {
    NEARBY_LOG(ERROR,
               "Attempted to resume the current EndpointChannel with  endpoint "
               "%s, but none was found",
               endpoint_id.c_str());
    return;
  }

  channel->Resume();

  // Report the success to the client
  client->OnBandwidthChanged(endpoint_id, channel->GetMedium());
}

void BwuManager::ProcessUpgradeFailureEvent(
    ClientProxy* client, const std::string& endpoint_id,
    const UpgradePathInfo& upgrade_info) {
  // The remote device failed to upgrade to the new medium we set up for them.
  // That's alright! We'll just try the next available medium (if there is
  // one).
  in_progress_upgrades_.erase(endpoint_id);

  // The first thing we have to do is to replace our
  // currentBwuMedium with the next best upgrade medium we share
  // with the remote device. The catch is that we can only do this if we only
  // have one connected endpoint. Otherwise, we'll end up disrupting our other
  // connected peers.
  if (channel_manager_->GetConnectedEndpointsCount() > 1) {
    // We can't change the currentBwuMedium, so there are no more
    // upgrade attempts for this endpoint. Sorry.
    NEARBY_LOG(
        ERROR,
        "Failed to attempt a new bandwidth upgrade for endpoint %s because we "
        "have other connected endpoints and can't try a new upgrade medium.",
        endpoint_id.c_str());
    return;
  }

  // Revert the existing upgrade medium for now.
  if (medium_ != Medium::UNKNOWN_MEDIUM) {
    Revert();
  }

  // Loop through the ordered list of upgrade mediums. One by one, remove the
  // top element until we get to the medium we last attempted to upgrade to.
  // The remainder of the list will contain the mediums we haven't attempted
  // yet.
  Medium last = parser::UpgradePathInfoMediumToMedium(upgrade_info.medium());
  std::vector<Medium> all_possible_mediums =
      client->GetUpgradeMediums(endpoint_id).GetMediums(true);
  std::vector<Medium> untried_mediums(all_possible_mediums);
  for (Medium medium : all_possible_mediums) {
    untried_mediums.erase(untried_mediums.begin());
    if (medium == last) {
      break;
    }
  }

  RetryUpgradeMediums(client, endpoint_id, untried_mediums);
}

void BwuManager::RetryUpgradeMediums(ClientProxy* client,
                                     const std::string& endpoint_id,
                                     std::vector<Medium> upgrade_mediums) {
  Medium next_medium = ChooseBestUpgradeMedium(upgrade_mediums);

  // If current medium is not WiFi and we have not succeeded with upgrading
  // yet, retry upgrade.
  Medium current_medium = GetEndpointMedium(endpoint_id);
  if (current_medium != Medium::WIFI_LAN &&
      (next_medium == current_medium || next_medium == Medium::UNKNOWN_MEDIUM ||
       upgrade_mediums.empty())) {
    RetryUpgradesAfterDelay(client, endpoint_id);
    return;
  }

  // Attempt to set the new upgrade medium.
  if (!SetCurrentBwuHandler(next_medium)) {
    NEARBY_LOG(
        INFO,
        "BwuManager failed to attempt a new bandwidth upgrade for endpoint %s "
        "because we couldn't set a new bandwidth upgrade medium.",
        endpoint_id.c_str());
    return;
  }

  // Now that we've successfully picked a new upgrade medium to try,
  // re-initiate the bandwidth upgrade.
  NEARBY_LOG(INFO,
             "BwuManager is attempting to upgrade endpoint %s again with a new "
             " bandwidth upgrade medium.",
             endpoint_id.c_str());
  InitiateBwuForEndpoint(client, endpoint_id);
}

std::vector<Medium> BwuManager::StripOutUnavailableMediums(
    const std::vector<Medium>& mediums) {
  std::vector<Medium> available_mediums;
  for (Medium m : mediums) {
    bool available = false;
    switch (m) {
      case Medium::WIFI_LAN:
        available = mediums_->GetWifiLan().IsAvailable();
        break;
      case Medium::BLUETOOTH:
        available = mediums_->GetBluetoothClassic().IsAvailable();
        break;
      default:
        break;
    }
    if (available) {
      available_mediums.push_back(m);
    }
  }
  return available_mediums;
}

// Returns the optimal medium supported by both devices.
// Each medium in the passed in list is checked for its availability with the
// medium_manager_ to ensure that the chosen upgrade medium is supported and
// available locally before continuing the upgrade. Once we pick a medium, all
// future connections will use it too. eg. If we chose Wifi LAN, we'll attempt
// to upgrade the 2nd, 3rd, etc remote endpoints with Wifi LAN even if they're
// on a different network (or had a better medium). This is a quick and easy
// way to prevent mediums, like Wifi Hotspot, from interfering with active
// connections (although it's suboptimal for bandwidth throughput). When all
// endpoints disconnect, we reset the bandwidth upgrade medium.
Medium BwuManager::ChooseBestUpgradeMedium(const std::vector<Medium>& mediums) {
  auto available_mediums = StripOutUnavailableMediums(mediums);
  if (medium_ == Medium::UNKNOWN_MEDIUM) {
    if (!available_mediums.empty()) {
      // Case 1: This is our first time upgrading, and we have at least one
      // supported medium to choose from. Return the first medium in the list,
      // since they are ordered by preference.
      return available_mediums[0];
    }
    // Case 2: This is our first time upgrading, but there are no available
    // upgrade mediums. Fall through to returning UNKNOWN_MEDIUM at the
    // bottom.
    NEARBY_LOG(
        INFO,
        "Current upgrade medium is unset, but there are no common supported "
        "upgrade mediums.");
  } else {
    // Case 3: We have already upgraded, and there is a list of supported
    // mediums to check against. Return the current upgrade medium if it's in
    // the supported list.
    if (std::find(available_mediums.begin(), available_mediums.end(),
                  medium_) != available_mediums.end()) {
      return medium_;
    }
    // Case 4: We have already upgraded, but the current medium is not
    // supported by the remote endpoint (it's not in the list, or the list is
    // empty). Fall through and return Medium.UNKNOWN_MEDIUM because we cannot
    // continue with the current upgrade medium, and we are not allowed to
    // switch.
    NEARBY_LOG(
        INFO,
        "Current upgrade medium %s is not supported by the remote endpoint",
        medium_);
  }

  return Medium::UNKNOWN_MEDIUM;
}

void BwuManager::RetryUpgradesAfterDelay(ClientProxy* client,
                                         const std::string& endpoint_id) {
  absl::Duration delay = CalculateNextRetryDelay(endpoint_id);
  CancelRetryUpgradeAlarm(endpoint_id);
  CancelableAlarm alarm(
      "BWU alarm",
      [this, client, endpoint_id]() {
        RunOnBwuManagerThread([this, client, endpoint_id]() {
          if (!client->IsConnectedToEndpoint(endpoint_id)) {
            return;
          }
          RetryUpgradeMediums(
              client, endpoint_id,
              client->GetUpgradeMediums(endpoint_id).GetMediums(true));
        });
      },
      delay, &alarm_executor_);

  retry_upgrade_alarms_.emplace(endpoint_id,
                                std::make_pair(std::move(alarm), delay));
  NEARBY_LOGS(INFO) << "Retry bandwidth upgrade after " << delay;
}

absl::Duration BwuManager::CalculateNextRetryDelay(
    const std::string& endpoint_id) {
  auto item = retry_upgrade_alarms_.find(endpoint_id);
  auto initial_delay = config_.bandwidth_upgrade_retry_delay;
  auto delay = item == retry_upgrade_alarms_.end()
                   ? initial_delay
                   : item->second.second + initial_delay;
  return std::min(delay, config_.bandwidth_upgrade_retry_max_delay);
}

void BwuManager::CancelRetryUpgradeAlarm(const std::string& endpoint_id) {
  auto item = retry_upgrade_alarms_.extract(endpoint_id);
  if (item.empty()) return;
  auto& pair = item.mapped();
  pair.first.Cancel();
}

void BwuManager::CancelAllRetryUpgradeAlarms() {
  for (const auto& item : retry_upgrade_alarms_) {
    const std::string& endpoint_id = item.first;
    CancelRetryUpgradeAlarm(endpoint_id);
  }
}

Medium BwuManager::GetEndpointMedium(const std::string& endpoint_id) {
  auto channel = channel_manager_->GetChannelForEndpoint(endpoint_id);
  return channel == nullptr ? Medium::UNKNOWN_MEDIUM : channel->GetMedium();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
