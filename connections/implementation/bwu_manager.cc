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

#include "connections/implementation/bwu_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "absl/time/time.h"
#include "connections/implementation/bluetooth_bwu_handler.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/service_id_constants.h"
#ifdef NO_WEBRTC
#include "connections/implementation/webrtc_bwu_handler_stub.h"
#else
#include "connections/implementation/webrtc_bwu_handler.h"
#endif
#include "connections/implementation/wifi_direct_bwu_handler.h"
#include "connections/implementation/wifi_hotspot_bwu_handler.h"
#include "connections/implementation/wifi_lan_bwu_handler.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {

using ::location::nearby::connections::BandwidthUpgradeNegotiationFrame;
using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::V1Frame;
using ::location::nearby::proto::connections::DisconnectionReason;

// Required for C++ 14 support in Chrome
constexpr absl::Duration BwuManager::kReadClientIntroductionFrameTimeout;

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
    if (FeatureFlags::GetInstance().GetFlags().use_exp_backoff_in_bwu_retry) {
      config_.bandwidth_upgrade_retry_delay =
          FeatureFlags::GetInstance()
              .GetFlags()
              .bwu_retry_exp_backoff_initial_delay;
    } else {
      config_.bandwidth_upgrade_retry_delay = absl::Seconds(5);
    }
  }
  if (config_.bandwidth_upgrade_retry_max_delay == absl::ZeroDuration()) {
    if (FeatureFlags::GetInstance().GetFlags().use_exp_backoff_in_bwu_retry) {
      config_.bandwidth_upgrade_retry_max_delay =
          FeatureFlags::GetInstance()
              .GetFlags()
              .bwu_retry_exp_backoff_maximum_delay;
    } else {
      config_.bandwidth_upgrade_retry_max_delay = absl::Seconds(10);
    }
  }
  if (config_.allow_upgrade_to.All(false)) {
    config_.allow_upgrade_to.web_rtc = true;
    config_.allow_upgrade_to.wifi_lan = true;
    config_.allow_upgrade_to.wifi_hotspot = true;
  }
  if (!handlers.empty()) {
    handlers_ = std::move(handlers);
  } else {
    InitBwuHandlers();
  }

  // Register the offline frame processor.
  endpoint_manager_->RegisterFrameProcessor(
      V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION, this);
}

BwuManager::~BwuManager() {
  NEARBY_LOGS(INFO) << "BwuManager going down";
  Shutdown();
}

void BwuManager::InitBwuHandlers() {
  // Register the supported concrete BwuMedium implementations.
  BwuHandler::BwuNotifications notifications{
      .incoming_connection_cb =
          absl::bind_front(&BwuManager::OnIncomingConnection, this),
  };
  if (config_.allow_upgrade_to.wifi_hotspot) {
    handlers_.emplace(
        Medium::WIFI_HOTSPOT,
        std::make_unique<WifiHotspotBwuHandler>(*mediums_, notifications));
  }
  if (config_.allow_upgrade_to.wifi_direct) {
    handlers_.emplace(
        Medium::WIFI_DIRECT,
        std::make_unique<WifiDirectBwuHandler>(*mediums_, notifications));
  }
  if (config_.allow_upgrade_to.wifi_lan) {
    handlers_.emplace(Medium::WIFI_LAN, std::make_unique<WifiLanBwuHandler>(
                                            *mediums_, notifications));
  }
  if (config_.allow_upgrade_to.web_rtc) {
    handlers_.emplace(Medium::WEB_RTC, std::make_unique<WebrtcBwuHandler>(
                                           *mediums_, notifications));
  }
  if (config_.allow_upgrade_to.bluetooth) {
    handlers_.emplace(Medium::BLUETOOTH, std::make_unique<BluetoothBwuHandler>(
                                             *mediums_, notifications));
  }
}

void BwuManager::Shutdown() {
  NEARBY_LOGS(INFO) << "Initiating shutdown of BwuManager.";

  endpoint_manager_->UnregisterFrameProcessor(
      V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION, this);

  // Stop all the ongoing Runnables (as gracefully as possible).
  ShutdownExecutors();

  // After worker threads are down we became exclusive owners of data and
  // may access it from current thread.
  for (auto& item : previous_endpoint_channels_) {
    EndpointChannel* channel = item.second.get();
    if (!channel) continue;
    channel->Close(DisconnectionReason::SHUTDOWN);
  }

  CancelAllRetryUpgradeAlarms();
  medium_ = Medium::UNKNOWN_MEDIUM;
  endpoint_id_to_bwu_medium_.clear();
  for (auto& medium_handler_pair : handlers_) {
    assert(medium_handler_pair.second);
    medium_handler_pair.second->RevertInitiatorState();
  }
  handlers_.clear();

  NEARBY_LOGS(INFO) << "BwuManager has shut down.";
}

void BwuManager::MakeSingleThreadedForTesting() {
  is_single_threaded_for_testing_ = true;
}

void BwuManager::InvokeOnIncomingConnectionForTesting(
    ClientProxy* client,
    std::unique_ptr<BwuHandler::IncomingSocketConnection> mutable_connection) {
  OnIncomingConnection(client, std::move(mutable_connection));
}

void BwuManager::ShutdownExecutors() {
  alarm_executor_.Shutdown();
  serial_executor_.Shutdown();
}

void BwuManager::InitiateBwuForEndpoint(ClientProxy* client,
                                        const std::string& endpoint_id,
                                        Medium new_medium) {
  // Select the best medium if one is not provided.
  Medium proposed_medium =
      new_medium == Medium::UNKNOWN_MEDIUM
          ? ChooseBestUpgradeMedium(
                endpoint_id,
                client->GetUpgradeMediums(endpoint_id).GetMediums(true))
          : new_medium;

  RunOnBwuManagerThread("bwu-init", [this, client, endpoint_id,
                                     proposed_medium]() {
    NEARBY_LOGS(INFO) << "InitiateBwuForEndpoint for endpoint " << endpoint_id
                      << " with medium "
                      << location::nearby::proto::connections::Medium_Name(
                             proposed_medium);

    if (channel_manager_->isWifiLanConnected() &&
        (proposed_medium == Medium::WIFI_HOTSPOT)) {
      NEARBY_LOGS(INFO)
          << "Some endpoint is using WIFI_LAN and proposed upgrade medium is "
             "WIFI_HOTSPOT. Don't do the BWU because connecting to "
             "WIFI_HOTSPOT will destroy WIFI_LAN which will lead BWU fail and "
             "other endpoint connection fail";
      return;
    }

    SetBwuMediumForEndpoint(endpoint_id, proposed_medium);
    BwuHandler* handler = GetHandlerForMedium(proposed_medium);
    if (!handler) {
      NEARBY_LOGS(ERROR)
          << "BwuManager cannot initiate bandwidth upgrade for endpoint "
          << endpoint_id
          << " because the current BandwidthUpgradeMedium cannot be deduced.";
      return;
    }

    if (in_progress_upgrades_.contains(endpoint_id)) {
      NEARBY_LOGS(INFO)
          << "BwuManager is ignoring bandwidth upgrade for endpoint "
          << endpoint_id
          << " because we're already upgrading bandwidth for that endpoint.";

      return;
    }

    CancelRetryUpgradeAlarm(endpoint_id);

    auto channel = channel_manager_->GetChannelForEndpoint(endpoint_id);
    Medium channel_medium =
        channel ? channel->GetMedium() : Medium::UNKNOWN_MEDIUM;
    client->GetAnalyticsRecorder().OnBandwidthUpgradeStarted(
        endpoint_id, channel_medium, proposed_medium,
        location::nearby::proto::connections::INCOMING,
        client->GetConnectionToken(endpoint_id));
    if (channel == nullptr) {
      NEARBY_LOGS(INFO)
          << "BwuManager couldn't complete the upgrade for endpoint "
          << endpoint_id
          << " because it couldn't find an existing EndpointChannel for it.";
      client->GetAnalyticsRecorder().OnBandwidthUpgradeError(
          endpoint_id, location::nearby::proto::connections::CHANNEL_ERROR,
          location::nearby::proto::connections::NETWORK_AVAILABLE);
      return;
    }

    // Ignore requests where the medium we're upgrading to is the medium we're
    // already connected over. This can happen now that Bluetooth is both an
    // advertising medium and a potential bandwidth upgrade, and will continue
    // to be possible as we add other new advertising mediums like mDNS (WiFi
    // LAN). Very specifically, this happens now when a device uses
    // P2P_CLUSTER, connects over Bluetooth, and is not connected to LAN.
    // Bluetooth is the best medium, and we attempt to upgrade from Bluetooth
    // to Bluetooth.
    if (proposed_medium == channel->GetMedium()) {
      NEARBY_LOGS(INFO) << "BwuManager ignoring the upgrade for endpoint "
                        << endpoint_id
                        << " because it is already connected over medium "
                        << location::nearby::proto::connections::Medium_Name(
                               proposed_medium);
      return;
    }

    std::string service_id = channel->GetServiceId();
    ByteArray bytes = handler->InitializeUpgradedMediumForEndpoint(
        client, service_id, endpoint_id);

    // Because we grab the endpointChannel first thing, it is possible the
    // endpointChannel is stale by the time we attempt to write over it.
    if (bytes.Empty()) {
      NEARBY_LOGS(ERROR)
          << "BwuManager couldn't complete the upgrade for endpoint "
          << endpoint_id << " to medium "
          << location::nearby::proto::connections::Medium_Name(proposed_medium)
          << " because it failed to initialize the "
             "BWU_NEGOTIATION.UPGRADE_PATH_AVAILABLE OfflineFrame.";
      UpgradePathInfo info;
      info.set_medium(parser::MediumToUpgradePathInfoMedium(proposed_medium));

      ProcessUpgradeFailureEvent(client, endpoint_id, info);
      client->GetAnalyticsRecorder().OnBandwidthUpgradeError(
          endpoint_id, location::nearby::proto::connections::RESULT_IO_ERROR,
          location::nearby::proto::connections::NETWORK_AVAILABLE);
      return;
    }
    if (!channel->Write(bytes).Ok()) {
      NEARBY_LOGS(ERROR)
          << "BwuManager couldn't complete the upgrade for endpoint "
          << endpoint_id << " to medium "
          << location::nearby::proto::connections::Medium_Name(proposed_medium)
          << " because it failed to write the "
             "BWU_NEGOTIATION.UPGRADE_PATH_AVAILABLE OfflineFrame.";
      return;
    }

    NEARBY_LOGS(INFO)
        << "BwuManager successfully wrote the "
           "BWU_NEGOTIATION.UPGRADE_PATH_AVAILABLE OfflineFrame while "
           "upgrading endpoint "
        << endpoint_id << " to medium "
        << location::nearby::proto::connections::Medium_Name(proposed_medium);
    in_progress_upgrades_.emplace(endpoint_id, client);
  });
}

void BwuManager::OnIncomingFrame(OfflineFrame& frame,
                                 const std::string& endpoint_id,
                                 ClientProxy* client, Medium medium,
                                 PacketMetaData& packet_meta_data) {
  V1Frame::FrameType frame_type = parser::GetFrameType(frame);
  if (frame_type != V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION) return;

  auto bwu_frame = frame.v1().bandwidth_upgrade_negotiation();
  NEARBY_LOGS(INFO) << "OnIncomingFrame: bwu_frame="
                    << BandwidthUpgradeNegotiationFrame::EventType_Name(
                           bwu_frame.event_type())
                    << ", endpoint_id=" << endpoint_id << ", medium="
                    << location::nearby::proto::connections::Medium_Name(
                           medium);
  if (FeatureFlags::GetInstance().GetFlags().enable_async_bandwidth_upgrade) {
    RunOnBwuManagerThread(
        "bwu-on-incoming-frame", [this, client, endpoint_id, bwu_frame]() {
          OnBwuNegotiationFrame(client, bwu_frame, endpoint_id);
        });
  } else {
    CountDownLatch latch(1);
    RunOnBwuManagerThread("bwu-on-incoming-frame", [this, client, endpoint_id,
                                                    bwu_frame, &latch]() {
      OnBwuNegotiationFrame(client, bwu_frame, endpoint_id);
      latch.CountDown();
    });
    latch.Await();
  }
}

void BwuManager::OnEndpointDisconnect(ClientProxy* client,
                                      const std::string& service_id,
                                      const std::string& endpoint_id,
                                      CountDownLatch barrier) {
  NEARBY_LOGS(INFO)
      << "BwuManager has processed endpoint disconnection for endpoint "
      << endpoint_id;
  RunOnBwuManagerThread("bwu-on-endpoint-disconnect", [this, client, service_id,
                                                       endpoint_id,
                                                       barrier]() mutable {
    Medium medium = GetBwuMediumForEndpoint(endpoint_id);
    BwuHandler* handler = GetHandlerForMedium(medium);
    if (handler) {
      handler->OnEndpointDisconnect(client, endpoint_id);
    }

    auto item = previous_endpoint_channels_.extract(endpoint_id);
    if (!item.empty()) {
      auto old_channel = item.mapped();
      if (old_channel != nullptr) {
        old_channel->Close(DisconnectionReason::SHUTDOWN);
      }
    }
    in_progress_upgrades_.erase(endpoint_id);
    retry_delays_.erase(endpoint_id);
    CancelRetryUpgradeAlarm(endpoint_id);
    successfully_upgraded_endpoints_.erase(endpoint_id);

    // Note(nohle): I'm skeptical of the "<= 1", which seems like it should be
    // "== 0". Luckily, we will enable the flag by default, and it won't matter.
    if (FeatureFlags::GetInstance().GetFlags().support_multiple_bwu_mediums ||
        channel_manager_->GetConnectedEndpointsCount() <= 1) {
      RevertBwuMediumForEndpoint(service_id, endpoint_id);
    }
    barrier.CountDown();
  });
}

void BwuManager::RevertBwuMediumForEndpoint(const std::string& service_id,
                                            const std::string& endpoint_id) {
  Medium medium = GetBwuMediumForEndpoint(endpoint_id);

  // If |support_multiple_bwu_mediums| is disabled, we take a less fine-grained
  // approach and revert the handler for _all_ endpoints.
  if (!FeatureFlags::GetInstance().GetFlags().support_multiple_bwu_mediums) {
    NEARBY_LOGS(INFO) << "Reverting medium "
                      << location::nearby::proto::connections::Medium_Name(
                             medium)
                      << " for all endpoints for service " << service_id;
    medium_ = Medium::UNKNOWN_MEDIUM;
    BwuHandler* handler = GetHandlerForMedium(medium);
    if (!handler) return;

    handler->RevertInitiatorState();
    return;
  }

  NEARBY_LOGS(INFO) << "Reverting medium "
                    << location::nearby::proto::connections::Medium_Name(medium)
                    << " for service ID " << service_id << " and endpoint "
                    << endpoint_id;
  endpoint_id_to_bwu_medium_.erase(endpoint_id);

  BwuHandler* handler = GetHandlerForMedium(medium);
  if (!handler) {
    NEARBY_LOGS(INFO) << "No BWU handler can be found for "
                      << location::nearby::proto::connections::Medium_Name(
                             medium);
    return;
  }
  // If |service_id| isn't of the INITIATOR-upgrade format--for example, if this
  // is called by the RESPONDER--there is no need to call RevertInitiatorState
  // unless the BWU Medium is Hotspot. The client needs to disconnect from
  // Hotspot, then it can restore the previous AP connection right away.
  if (!IsInitiatorUpgradeServiceId(service_id)) {
    if (medium == Medium::WIFI_HOTSPOT || medium == Medium::WIFI_DIRECT) {
      handler->RevertResponderState(service_id);
    }
    return;
  }

  handler->RevertInitiatorState(service_id, endpoint_id);
}

bool BwuManager::IsUpgradeOngoing(const std::string& endpoint_id) {
    CountDownLatch latch(1);
    RunOnBwuManagerThread("is_upgrade_ongoing", [&latch]() {
      latch.CountDown();
    });
    latch.Await();
    return in_progress_upgrades_.contains(endpoint_id);
}

Medium BwuManager::GetBwuMediumForEndpoint(
    const std::string& endpoint_id) const {
  if (!FeatureFlags::GetInstance().GetFlags().support_multiple_bwu_mediums) {
    return medium_;
  }

  auto it = endpoint_id_to_bwu_medium_.find(endpoint_id);
  return it == endpoint_id_to_bwu_medium_.end() ? Medium::UNKNOWN_MEDIUM
                                                : it->second;
}
void BwuManager::SetBwuMediumForEndpoint(const std::string& endpoint_id,
                                         Medium medium) {
  if (!FeatureFlags::GetInstance().GetFlags().support_multiple_bwu_mediums) {
    medium_ = medium;
    return;
  }

  endpoint_id_to_bwu_medium_[endpoint_id] = medium;
}

BwuHandler* BwuManager::GetHandlerForMedium(Medium medium) const {
  if (medium == Medium::UNKNOWN_MEDIUM) return nullptr;

  auto it = handlers_.find(medium);
  if (it == handlers_.end()) return nullptr;

  return it->second.get();
}

void BwuManager::OnBwuNegotiationFrame(ClientProxy* client,
                                       const BwuNegotiationFrame frame,
                                       const string& endpoint_id) {
  NEARBY_LOGS(INFO) << "OnBwuNegotiationFrame: processing incoming "
                    << BwuNegotiationFrame::EventType_Name(frame.event_type())
                    << " frame for endpoint " << endpoint_id;
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
      NEARBY_LOGS(WARNING)
          << "BwuManager can't process unknown incoming OfflineFrame of type "
          << BandwidthUpgradeNegotiationFrame::EventType_Name(
                 frame.event_type())
          << ", ignoring it.";
      break;
  }
}

void BwuManager::OnIncomingConnection(
    ClientProxy* client,
    std::unique_ptr<BwuHandler::IncomingSocketConnection> mutable_connection) {
  NEARBY_LOGS(INFO) << "BwuManager process incoming connection";
  std::shared_ptr<BwuHandler::IncomingSocketConnection> connection(
      mutable_connection.release());
  RunOnBwuManagerThread(
      "bwu-on-incoming-connection", [this, client, connection]() {
        absl::Time connection_attempt_start_time =
            SystemClock::ElapsedRealtime();
        EndpointChannel* channel = connection->channel.get();
        if (channel == nullptr) {
          NEARBY_LOGS(ERROR)
              << "BwuManager failed to create new EndpointChannel for incoming "
                 "socket.";
          connection->socket->Close();
          AttemptToRecordBandwidthUpgradeErrorForUnknownEndpoint(
              location::nearby::proto::connections::MEDIUM_ERROR,
              location::nearby::proto::connections::SOCKET_CREATION);
          return;
        }

        NEARBY_LOGS(VERBOSE)
            << "BwuManager successfully created new EndpointChannel for "
               "incoming socket";

        ClientIntroduction introduction;
        if (!ReadClientIntroductionFrame(channel, introduction)) {
          // This was never a fully EstablishedConnection, no need to provide a
          // closure reason.
          channel->Close();
          NEARBY_LOGS(ERROR)
              << "BwuManager failed to read "
                 "BWU_NEGOTIATION.CLIENT_INTRODUCTION OfflineFrame from "
                 "newly-created EndpointChannel "
              << channel->GetName()
              << ", so the EndpointChannel was discarded.";
          return;
        }

        if (!WriteClientIntroductionAckFrame(channel)) {
          // This was never a fully EstablishedConnection, no need to provide a
          // closure reason.
          channel->Close();
          return;
        }

        NEARBY_LOGS(VERBOSE) << "BwuManager successfully received "
                                "BWU_NEGOTIATION.CLIENT_INTRODUCTION "
                                "OfflineFrame on EndpointChannel "
                             << channel->GetName();

        const std::string& endpoint_id = introduction.endpoint_id();
        ClientProxy* mapped_client;
        const auto item = in_progress_upgrades_.find(endpoint_id);
        if (item == in_progress_upgrades_.end()) return;
        mapped_client = item->second;
        CancelRetryUpgradeAlarm(endpoint_id);
        if (mapped_client == nullptr) {
          // This was never a fully EstablishedConnection, no need to provide a
          // closure reason.
          channel->Close();
          return;
        }

        CHECK(client == mapped_client);

        // The ConnectionAttempt has now succeeded, so record it as such.
        std::unique_ptr<ConnectionAttemptMetadataParams>
            connections_attempt_metadata_params;
        if (channel != nullptr) {
          connections_attempt_metadata_params =
              client->GetAnalyticsRecorder()
                  .BuildConnectionAttemptMetadataParams(
                      channel->GetTechnology(), channel->GetBand(),
                      channel->GetFrequency(), channel->GetTryCount());
        }
        client->GetAnalyticsRecorder().OnIncomingConnectionAttempt(
            location::nearby::proto::connections::UPGRADE, channel->GetMedium(),
            location::nearby::proto::connections::RESULT_SUCCESS,
            SystemClock::ElapsedRealtime() - connection_attempt_start_time,
            client->GetConnectionToken(endpoint_id),
            connections_attempt_metadata_params.get());

        // Use the introductory client information sent over to run the upgrade
        // protocol.
        RunUpgradeProtocol(mapped_client, endpoint_id,
                           std::move(connection->channel),
                           !introduction.supports_disabling_encryption());
      });
}

void BwuManager::RunOnBwuManagerThread(const std::string& name,
                                       Runnable runnable) {
  if (is_single_threaded_for_testing_) {
    runnable();
    return;
  }

  serial_executor_.Execute(name, std::move(runnable));
}

void BwuManager::RunUpgradeProtocol(
    ClientProxy* client, const std::string& endpoint_id,
    std::unique_ptr<EndpointChannel> new_channel, bool enable_encryption) {
  NEARBY_LOGS(INFO) << "RunUpgradeProtocol new channel @" << new_channel.get()
                    << " name: " << new_channel->GetName() << ", medium: "
                    << location::nearby::proto::connections::Medium_Name(
                           new_channel->GetMedium());
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
  if (!old_channel) {
    NEARBY_LOGS(INFO)
        << "BwuManager didn't find a previous EndpointChannel for "
        << endpoint_id
        << " when registering the new EndpointChannel, short-circuiting the "
           "upgrade protocol.";
    client->GetAnalyticsRecorder().OnBandwidthUpgradeError(
        endpoint_id, location::nearby::proto::connections::CHANNEL_ERROR,
        location::nearby::proto::connections::PRIOR_ENDPOINT_CHANNEL);
    return;
  }
  channel_manager_->ReplaceChannelForEndpoint(
      client, endpoint_id, std::move(new_channel), enable_encryption);

  // Next, initiate a clean shutdown for the previous EndpointChannel used for
  // this endpoint by telling the remote device that it will not receive any
  // more writes over that EndpointChannel.
  if (!old_channel->Write(parser::ForBwuLastWrite()).Ok()) {
    NEARBY_LOGS(ERROR)
        << "BwuManager failed to write "
           "BWU_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL OfflineFrame to "
           "endpoint "
        << endpoint_id << ", short-circuiting the upgrade protocol.";
    client->GetAnalyticsRecorder().OnBandwidthUpgradeError(
        endpoint_id, location::nearby::proto::connections::RESULT_IO_ERROR,
        location::nearby::proto::connections::LAST_WRITE_TO_PRIOR_CHANNEL);
    return;
  }
  NEARBY_LOGS(VERBOSE) << "BwuManager successfully wrote "
                          "BWU_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL "
                          "OfflineFrame while upgrading endpoint "
                       << endpoint_id;

  // The remainder of this clean shutdown for the previous EndpointChannel will
  // continue when we receive a corresponding
  // BANDWIDTH_UPGRADE_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL OfflineFrame from
  // the remote device, so for now, just store that previous EndpointChannel.
  previous_endpoint_channels_.emplace(endpoint_id, old_channel);

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
  Medium upgrade_medium =
      parser::UpgradePathInfoMediumToMedium(upgrade_path_info.medium());
  NEARBY_LOGS(INFO) << "ProcessBwuPathAvailableEvent for endpoint "
                    << endpoint_id << " medium "
                    << location::nearby::proto::connections::Medium_Name(
                           upgrade_medium);

  if (channel_manager_->isWifiLanConnected() &&
      (upgrade_medium == Medium::WIFI_HOTSPOT)) {
    NEARBY_LOGS(INFO)
        << "Some endpoint is using WIFI_LAN and proposed upgrade medium is "
           "WIFI_HOTSPOT. Don't do the BWU because connecting to "
           "WIFI_HOTSPOT will destroy WIFI_LAN which will lead BWU fail and "
           "other endpoint connection fail";
    RunUpgradeFailedProtocol(client, endpoint_id, upgrade_path_info);
    return;
  }

  if (in_progress_upgrades_.contains(endpoint_id)) {
    NEARBY_LOGS(ERROR)
        << "BwuManager received a duplicate bandwidth upgrade for endpoint "
        << endpoint_id
        << ". We're out of sync with the remote device and cannot recover; "
           "closing all channels.";

    auto item = previous_endpoint_channels_.extract(endpoint_id);
    if (!item.empty()) {
      std::shared_ptr<EndpointChannel> previous_endpoint_channel =
          item.mapped();
      if (previous_endpoint_channel) {
        previous_endpoint_channel->Close(DisconnectionReason::UNFINISHED);
      }
    }
    std::shared_ptr<EndpointChannel> new_channel =
        channel_manager_->GetChannelForEndpoint(endpoint_id);
    if (new_channel) {
      // The upgraded channel never finished upgrading, and therefore is still
      // paused.
      new_channel->Resume();
      new_channel->Close(DisconnectionReason::UNFINISHED);
    }

    return;
  }
  Medium current_medium = GetBwuMediumForEndpoint(endpoint_id);
  if (current_medium == Medium::UNKNOWN_MEDIUM) {
    SetBwuMediumForEndpoint(endpoint_id, upgrade_medium);
  }
  // Check for the correct medium so we don't process an incorrect OfflineFrame.
  if (upgrade_medium != GetBwuMediumForEndpoint(endpoint_id)) {
    NEARBY_LOGS(INFO) << "Medium not matching";
    RunUpgradeFailedProtocol(client, endpoint_id, upgrade_path_info);
    return;
  }

  client->GetAnalyticsRecorder().OnBandwidthUpgradeStarted(
      endpoint_id, current_medium, upgrade_medium,
      location::nearby::proto::connections::OUTGOING,
      client->GetConnectionToken(endpoint_id));

  absl::Time connection_attempt_start_time = SystemClock::ElapsedRealtime();
  auto channel = ProcessBwuPathAvailableEventInternal(client, endpoint_id,
                                                      upgrade_path_info);
  location::nearby::proto::connections::ConnectionAttemptResult
      connection_attempt_result;
  if (channel != nullptr) {
    connection_attempt_result =
        location::nearby::proto::connections::RESULT_SUCCESS;
  } else if (client->GetCancellationFlag(endpoint_id)->Cancelled()) {
    connection_attempt_result =
        location::nearby::proto::connections::RESULT_CANCELLED;
    client->GetAnalyticsRecorder().OnBandwidthUpgradeError(
        endpoint_id, location::nearby::proto::connections::RESULT_REMOTE_ERROR,
        location::nearby::proto::connections::UPGRADE_CANCEL);
  } else {
    connection_attempt_result =
        location::nearby::proto::connections::RESULT_ERROR;
  }

  std::unique_ptr<ConnectionAttemptMetadataParams>
      connections_attempt_metadata_params;
  if (channel != nullptr) {
    connections_attempt_metadata_params =
        client->GetAnalyticsRecorder().BuildConnectionAttemptMetadataParams(
            channel->GetTechnology(), channel->GetBand(),
            channel->GetFrequency(), channel->GetTryCount());
  }
  client->GetAnalyticsRecorder().OnOutgoingConnectionAttempt(
      endpoint_id, location::nearby::proto::connections::UPGRADE,
      upgrade_medium, connection_attempt_result,
      SystemClock::ElapsedRealtime() - connection_attempt_start_time,
      client->GetConnectionToken(endpoint_id),
      connections_attempt_metadata_params.get());

  if (channel == nullptr) {
    NEARBY_LOGS(INFO) << "Failed to get new channel.";
    RunUpgradeFailedProtocol(client, endpoint_id, upgrade_path_info);
    return;
  }

  in_progress_upgrades_.emplace(endpoint_id, client);
  RunUpgradeProtocol(client, endpoint_id, std::move(channel),
                     !upgrade_path_info.supports_disabling_encryption());
}

std::unique_ptr<EndpointChannel>
BwuManager::ProcessBwuPathAvailableEventInternal(
    ClientProxy* client, const string& endpoint_id,
    const UpgradePathInfo& upgrade_path_info) {
  Medium medium =
      parser::UpgradePathInfoMediumToMedium(upgrade_path_info.medium());
  if (medium != GetBwuMediumForEndpoint(endpoint_id)) {
    NEARBY_LOGS(ERROR)
        << "ProcessBwuPathAvailableEventInternal failed for endpoint "
        << endpoint_id << " medium "
        << location::nearby::proto::connections::Medium_Name(medium)
        << ". Upgrade medium not yet set for endpoint.";
    return nullptr;
  }

  BwuHandler* handler = GetHandlerForMedium(medium);
  if (!handler) {
    NEARBY_LOGS(ERROR)
        << "ProcessBwuPathAvailableEventInternal failed for endpoint "
        << endpoint_id << " medium "
        << location::nearby::proto::connections::Medium_Name(medium)
        << ". No handler for medium.";
    return nullptr;
  }

  NEARBY_LOGS(INFO) << "ProcessBwuPathAvailableEventInternal for "
                       "endpoint "
                    << endpoint_id << " medium "
                    << location::nearby::proto::connections::Medium_Name(
                           medium);

  // Get service ID from the old channel. Don't keep the old channel's shared
  // pointer in scope longer than necessary.
  std::string service_id;
  {
    std::shared_ptr<EndpointChannel> old_channel =
        channel_manager_->GetChannelForEndpoint(endpoint_id);
    if (!old_channel) {
      NEARBY_LOGS(ERROR)
          << "ProcessBwuPathAvailableEventInternal failed for endpoint "
          << endpoint_id << " medium "
          << location::nearby::proto::connections::Medium_Name(medium)
          << ". Old endpoint channel is missing.";
      return nullptr;
    }
    service_id = old_channel->GetServiceId();
  }

  std::unique_ptr<EndpointChannel> new_channel =
      handler->CreateUpgradedEndpointChannel(client, service_id, endpoint_id,
                                             upgrade_path_info);
  if (!new_channel) {
    NEARBY_LOGS(ERROR) << "BwuManager failed to create an endpoint "
                          "channel to endpoint"
                       << endpoint_id << ", aborting upgrade.";
    client->GetAnalyticsRecorder().OnBandwidthUpgradeError(
        endpoint_id, location::nearby::proto::connections::RESULT_IO_ERROR,
        location::nearby::proto::connections::SOCKET_CREATION);
    return nullptr;
  }

  // Write the requisite BANDWIDTH_UPGRADE_NEGOTIATION.CLIENT_INTRODUCTION as
  // the first OfflineFrame on this new EndpointChannel.
  if (!new_channel
           ->Write(parser::ForBwuIntroduction(
               client->GetLocalEndpointId(),
               upgrade_path_info.supports_disabling_encryption()))
           .Ok()) {
    // This was never a fully EstablishedConnection, no need to provide a
    // closure reason.
    new_channel->Close();

    NEARBY_LOGS(ERROR)
        << "BwuManager failed to write BWU_NEGOTIATION.CLIENT_INTRODUCTION "
           "OfflineFrame to newly-created EndpointChannel "
        << new_channel->GetName() << ", aborting upgrade.";
    client->GetAnalyticsRecorder().OnBandwidthUpgradeError(
        endpoint_id, location::nearby::proto::connections::RESULT_IO_ERROR,
        location::nearby::proto::connections::CLIENT_INTRODUCTION);
    return {};
  }

  if (upgrade_path_info.supports_client_introduction_ack()) {
    if (!ReadClientIntroductionAckFrame(new_channel.get())) {
      // This was never a fully EstablishedConnection, no need to provide a
      // closure reason.
      new_channel->Close();

      NEARBY_LOGS(ERROR) << "BwuManager failed to read "
                            "BWU_NEGOTIATION.CLIENT_INTRODUCTION_ACK "
                            "OfflineFrame to newly-created EndpointChannel "
                         << new_channel->GetName() << ", aborting upgrade.";

      return {};
    }
  }

  NEARBY_LOGS(INFO) << "BwuManager successfully wrote "
                       "BWU_NEGOTIATION.CLIENT_INTRODUCTION OfflineFrame to "
                       "newly-created EndpointChannel "
                    << new_channel->GetName() << " while upgrading endpoint "
                    << endpoint_id;

  // Set the AnalyticsRecorder so that the future closure of this
  // EndpointChannel will be recorded.
  return new_channel;
}

void BwuManager::RunUpgradeFailedProtocol(
    ClientProxy* client, const std::string& endpoint_id,
    const UpgradePathInfo& upgrade_path_info) {
  NEARBY_LOGS(INFO) << "RunUpgradeFailedProtocol for endpoint " << endpoint_id
                    << " medium "
                    << location::nearby::proto::connections::Medium_Name(
                           parser::UpgradePathInfoMediumToMedium(
                               upgrade_path_info.medium()));
  // We attempted to connect to the new medium that the remote device has set up
  // for us but we failed. We need to let the remote device know so that they
  // can pick another medium for us to try.
  std::shared_ptr<EndpointChannel> channel =
      channel_manager_->GetChannelForEndpoint(endpoint_id);
  if (!channel) {
    NEARBY_LOGS(ERROR)
        << "BwuManager didn't find a previous EndpointChannel for "
        << endpoint_id
        << " when sending an upgrade failure frame, short-circuiting the "
           "upgrade protocol.";
    client->GetAnalyticsRecorder().OnBandwidthUpgradeError(
        endpoint_id, location::nearby::proto::connections::CHANNEL_ERROR,
        location::nearby::proto::connections::NETWORK_AVAILABLE);
    return;
  }

  // Report UPGRADE_FAILURE to the remote device.
  if (!channel->Write(parser::ForBwuFailure(upgrade_path_info)).Ok()) {
    channel->Close(DisconnectionReason::IO_ERROR);

    NEARBY_LOGS(ERROR)
        << "BwuManager failed to write BWU_NEGOTIATION.UPGRADE_FAILURE "
           "OfflineFrame to endpoint "
        << endpoint_id << ", short-circuiting the upgrade protocol.";
    client->GetAnalyticsRecorder().OnBandwidthUpgradeError(
        endpoint_id, location::nearby::proto::connections::RESULT_IO_ERROR,
        location::nearby::proto::connections::NETWORK_AVAILABLE);
    return;
  }

  // And lastly, clean up our medium since we failed to utilize it anyways.
  if (GetBwuMediumForEndpoint(endpoint_id) != Medium::UNKNOWN_MEDIUM) {
    RevertBwuMediumForEndpoint(channel->GetServiceId(), endpoint_id);
  }
  in_progress_upgrades_.erase(endpoint_id);
  NEARBY_LOGS(INFO) << "BwuManager has informed endpoint " << endpoint_id
                    << " that the bandwidth upgrade failed.";
}

bool BwuManager::ReadClientIntroductionFrame(EndpointChannel* channel,
                                             ClientIntroduction& introduction) {
  NEARBY_LOGS(INFO) << "ReadClientIntroductionFrame with channel name: "
                    << channel->GetName() << ", medium: "
                    << location::nearby::proto::connections::Medium_Name(
                           channel->GetMedium());
  CancelableAlarm timeout_alarm(
      "BwuManager::ReadClientIntroductionFrame",
      [channel]() {
        NEARBY_LOGS(ERROR) << "In BwuManager, failed to read the "
                              "ClientIntroductionFrame after "
                           << absl::FormatDuration(
                                  kReadClientIntroductionFrameTimeout)
                           << ". Timing out and closing EndpointChannel "
                           << channel->GetType();
        channel->Close();
      },
      kReadClientIntroductionFrameTimeout, &alarm_executor_);
  auto data = channel->Read();
  timeout_alarm.Cancel();
  if (!data.ok()) return false;
  auto transfer(parser::FromBytes(data.result()));
  if (!transfer.ok()) {
    NEARBY_LOGS(ERROR) << "In ReadClientIntroductionFrame, attempted to read a "
                          "ClientIntroductionFrame from EndpointChannel "
                       << channel->GetType()
                       << " but was unable to obtain any OfflineFrame.";
    return false;
  }
  OfflineFrame frame = transfer.result();
  if (!frame.has_v1() || !frame.v1().has_bandwidth_upgrade_negotiation()) {
    NEARBY_LOGS(ERROR)
        << "In ReadClientIntroductionFrame, expected a "
           "BANDWIDTH_UPGRADE_NEGOTIATION v1 OfflineFrame but got a "
        << parser::GetFrameType(frame) << " frame instead.";
    return false;
  }
  if (frame.v1().bandwidth_upgrade_negotiation().event_type() !=
      BandwidthUpgradeNegotiationFrame::CLIENT_INTRODUCTION) {
    NEARBY_LOGS(ERROR)
        << "In ReadClientIntroductionFrame, expected a CLIENT_INTRODUCTION "
           "v1 OfflineFrame but got a BANDWIDTH_UPGRADE_NEGOTIATION frame "
           "with eventType "
        << frame.v1().bandwidth_upgrade_negotiation().event_type()
        << " instead.";
    return false;
  }
  const auto& frame_intro =
      frame.v1().bandwidth_upgrade_negotiation().client_introduction();
  introduction = frame_intro;
  return true;
}

bool BwuManager::ReadClientIntroductionAckFrame(EndpointChannel* channel) {
  NEARBY_LOGS(INFO) << "ReadClientIntroductionAckFrame with channel name: "
                    << channel->GetName() << ", medium: "
                    << location::nearby::proto::connections::Medium_Name(
                           channel->GetMedium());
  CancelableAlarm timeout_alarm(
      "BwuManager::ReadClientIntroductionAckFrame",
      [channel]() {
        NEARBY_LOGS(ERROR)
            << "In BwuManager, failed to read the ClientIntroductionAckFrame "
               "after "
            << absl::FormatDuration(kReadClientIntroductionFrameTimeout)
            << ". Timing out and closing EndpointChannel "
            << channel->GetType();
        channel->Close();
      },
      kReadClientIntroductionFrameTimeout, &alarm_executor_);
  auto data = channel->Read();
  timeout_alarm.Cancel();
  if (!data.ok()) return false;
  auto transfer(parser::FromBytes(data.result()));
  if (!transfer.ok()) return false;
  OfflineFrame frame = transfer.result();
  if (!frame.has_v1() || !frame.v1().has_bandwidth_upgrade_negotiation())
    return false;
  if (frame.v1().bandwidth_upgrade_negotiation().event_type() !=
      BandwidthUpgradeNegotiationFrame::CLIENT_INTRODUCTION_ACK)
    return false;
  return true;
}

bool BwuManager::WriteClientIntroductionAckFrame(EndpointChannel* channel) {
  NEARBY_LOGS(INFO) << "WriteClientIntroductionAckFrame channel name: "
                    << channel->GetName() << ", medium: "
                    << location::nearby::proto::connections::Medium_Name(
                           channel->GetMedium());
  return channel->Write(parser::ForBwuIntroductionAck()).Ok();
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
    NEARBY_LOGS(ERROR)
        << "BwuManager received a BWU_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL "
           "OfflineFrame for unknown endpoint "
        << endpoint_id << ", can't complete the upgrade protocol.";
    successfully_upgraded_endpoints_.emplace(endpoint_id);
    return;
  }

  NEARBY_LOGS(INFO) << "ProcessLastWriteToPriorChannelEvent: service_id="
                    << previous_endpoint_channel->GetServiceId()
                    << ", endpoint_id=" << endpoint_id << ", medium="
                    << location::nearby::proto::connections::Medium_Name(
                           previous_endpoint_channel->GetMedium());

  if (!previous_endpoint_channel->Write(parser::ForBwuSafeToClose()).Ok()) {
    previous_endpoint_channel->Close(DisconnectionReason::IO_ERROR);
    // Remove this prior EndpointChannel from previous_endpoint_channels to
    // avoid leaks.
    previous_endpoint_channels_.erase(endpoint_id);

    NEARBY_LOGS(ERROR) << "BwuManager failed to write "
                          "BWU_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL "
                          "OfflineFrame to endpoint "
                       << endpoint_id
                       << ", short-circuiting the upgrade protocol.";
    client->GetAnalyticsRecorder().OnBandwidthUpgradeError(
        endpoint_id, location::nearby::proto::connections::RESULT_IO_ERROR,
        location::nearby::proto::connections::SAFE_TO_CLOSE_PRIOR_CHANNEL);
    return;
  }
  NEARBY_LOGS(VERBOSE) << "BwuManager successfully wrote "
                          "BWU_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL "
                          "OfflineFrame while trying to upgrade endpoint "
                       << endpoint_id;

  // The upgrade protocol's clean shutdown of the prior EndpointChannel will
  // conclude when we receive a corresponding
  // BANDWIDTH_UPGRADE_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL OfflineFrame
  // from the remote device.
}

void BwuManager::ProcessSafeToClosePriorChannelEvent(
    ClientProxy* client, const std::string& endpoint_id) {
  NEARBY_LOGS(INFO) << "ProcessSafeToClosePriorChannelEvent for endpoint "
                    << endpoint_id;
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
    NEARBY_LOGS(ERROR)
        << "BwuManager received a BWU_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL "
           "OfflineFrame for unknown endpoint "
        << endpoint_id << ", can't complete the upgrade protocol.";
    return;
  }
  NEARBY_LOGS(INFO)
      << "BwuManager successfully received a "
      << "BWU_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL OfflineFrame while "
      << "trying to upgrade endpoint " << endpoint_id;

  // Each encrypted message includes the key to decrypt the next message. The
  // disconnect message is optional and may not be received under normal
  // circumstances so it is necessary to send it unencrypted. This way the
  // serial crypto context does not increment here.
  previous_endpoint_channel->DisableEncryption();
  previous_endpoint_channel->Write(parser::ForDisconnection());

  // Attempt to read the disconnect message from the previous channel. We don't
  // care whether we successfully read it or whether we get an exception here.
  // The idea is just to make sure the other side has had a chance to receive
  // the full SAFE_TO_CLOSE_PRIOR_CHANNEL message before we actually close the
  // channel. See b/172380349 for more context.
  previous_endpoint_channel->Read();
  previous_endpoint_channel->Close(DisconnectionReason::UPGRADED);

  NEARBY_LOGS(VERBOSE)
      << "BwuManager cleanly shut down prior "
      << previous_endpoint_channel->GetType()
      << " EndpointChannel to conclude upgrade protocol for endpoint "
      << endpoint_id;

  // Now the upgrade protocol has completed, record analytics for this new
  // upgraded bandwidth connection...
  client->GetAnalyticsRecorder().OnConnectionEstablished(
      endpoint_id, GetBwuMediumForEndpoint(endpoint_id),
      client->GetConnectionToken(endpoint_id));
  // ...and the success of the upgrade itself.
  client->GetAnalyticsRecorder().OnBandwidthUpgradeSuccess(endpoint_id);

  // Now that the old channel has been drained, we can unpause the new channel
  std::shared_ptr<EndpointChannel> channel =
      channel_manager_->GetChannelForEndpoint(endpoint_id);

  if (!channel) {
    NEARBY_LOGS(ERROR) << "BwuManager attempted to resume the current "
                          "EndpointChannel with endpoint "
                       << endpoint_id << ", but none was found.";
    return;
  }

  channel->Resume();

  // Report the success to the client
  client->OnBandwidthChanged(endpoint_id, channel->GetMedium());
  in_progress_upgrades_.erase(endpoint_id);
}

void BwuManager::ProcessUpgradeFailureEvent(
    ClientProxy* client, const std::string& endpoint_id,
    const UpgradePathInfo& upgrade_info) {
  NEARBY_LOGS(INFO) << "ProcessUpgradeFailureEvent for endpoint " << endpoint_id
                    << " from medium: "
                    << location::nearby::proto::connections::Medium_Name(
                           parser::UpgradePathInfoMediumToMedium(
                               upgrade_info.medium()));
  // The remote device failed to upgrade to the new medium we set up for them.
  // That's alright! We'll just try the next available medium (if there is one).
  in_progress_upgrades_.erase(endpoint_id);

  // The first thing we have to do is to replace our currentBwuMedium with the
  // next best upgrade medium we share with the remote device. The catch is that
  // we can only do this if we only have one connected endpoint. Otherwise,
  // we'll end up disrupting our other connected peers.
  // Note: This is only necessary when |support_multiple_bwu_mediums| is
  // disabled.
  if (!FeatureFlags::GetInstance().GetFlags().support_multiple_bwu_mediums &&
      channel_manager_->GetConnectedEndpointsCount() > 1) {
    // We can't change the currentBwuMedium, so there are no more upgrade
    // attempts for this endpoint. Sorry.
    NEARBY_LOGS(ERROR)
        << "BwuManager failed to attempt a new bandwidth upgrade for endpoint "
        << endpoint_id
        << " because we have other connected endpoints and can't try a new "
           "upgrade medium.";
    client->GetAnalyticsRecorder().OnBandwidthUpgradeError(
        endpoint_id, location::nearby::proto::connections::CHANNEL_ERROR,
        location::nearby::proto::connections::NETWORK_AVAILABLE);
    return;
  }

  EndpointChannel* channel =
      channel_manager_->GetChannelForEndpoint(endpoint_id).get();
  std::string service_id =
      channel ? channel->GetServiceId() : std::string(kUnknownServiceId);
  // Revert the existing upgrade medium for now.
  if (GetBwuMediumForEndpoint(endpoint_id) != Medium::UNKNOWN_MEDIUM) {
    // This is the BWU initiator, so append "_UPGRADE" to service_id.
    // Otherwise the Revert logic won't call into platform medium layer code.
    std::string upgrade_service_id = WrapInitiatorUpgradeServiceId(service_id);
    RevertBwuMediumForEndpoint(upgrade_service_id, endpoint_id);
  }

  // Loop through the ordered list of upgrade mediums. One by one, remove the
  // top element until we get to the medium we last attempted to upgrade to. The
  // remainder of the list will contain the mediums we haven't attempted yet.
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

  TryNextBestUpgradeMediums(client, endpoint_id, untried_mediums);
}

void BwuManager::TryNextBestUpgradeMediums(
    ClientProxy* client, const std::string& endpoint_id,
    std::vector<Medium> upgrade_mediums) {
  Medium next_medium = ChooseBestUpgradeMedium(endpoint_id, upgrade_mediums);
  NEARBY_LOGS(INFO) << "Try Next Best Medium for endpoint " << endpoint_id
                    << " after ChooseBestUpgradeMedium: "
                    << location::nearby::proto::connections::Medium_Name(
                           next_medium);

  // If current medium is not WiFi and we have not succeeded with upgrading yet,
  // retry upgrade.
  // TODO(b/228610864): Do we actually want to treat WIFI_LAN differently?
  auto channel = channel_manager_->GetChannelForEndpoint(endpoint_id);
  Medium current_medium =
      channel ? channel->GetMedium() : Medium::UNKNOWN_MEDIUM;
  NEARBY_LOGS(VERBOSE) << "current_medium: "
                       << location::nearby::proto::connections::Medium_Name(
                              current_medium);
  if (current_medium != Medium::WIFI_LAN &&
      (next_medium == current_medium || next_medium == Medium::UNKNOWN_MEDIUM ||
       upgrade_mediums.empty())) {
    RetryUpgradesAfterDelay(client, endpoint_id);
    return;
  }

  // Attempt to set the new upgrade medium.
  if (!GetHandlerForMedium(next_medium)) {
    // As Medium without handler has been stripped out, this shouldn't be hit
    NEARBY_LOGS(INFO)
        << "BwuManager failed to attempt a new bandwidth upgrade for endpoint "
        << endpoint_id
        << " because we couldn't set a new bandwidth upgrade medium.";
    return;
  }
  SetBwuMediumForEndpoint(endpoint_id, next_medium);

  // Now that we've successfully picked a new upgrade medium to try,
  // re-initiate the bandwidth upgrade.
  NEARBY_LOGS(INFO) << "BwuManager is attempting to upgrade endpoint "
                    << endpoint_id
                    << " again with a new bandwidth upgrade medium.";
  InitiateBwuForEndpoint(client, endpoint_id, next_medium);
}

std::vector<Medium> BwuManager::StripOutUnavailableMediums(
    const std::vector<Medium>& mediums) const {
  std::vector<Medium> available_mediums;
  for (Medium m : mediums) {
    bool available = false;
    if (GetHandlerForMedium(m)) {
      switch (m) {
        case Medium::WIFI_LAN:
          available = mediums_->GetWifiLan().IsAvailable();
          break;
        case Medium::WIFI_DIRECT:
          available = mediums_->GetWifiDirect().IsGOAvailable();
          break;
        case Medium::WIFI_HOTSPOT:
          available = mediums_->GetWifiHotspot().IsAPAvailable();
          break;
        case Medium::WEB_RTC:
          available = mediums_->GetWebRtc().IsAvailable();
          break;
        case Medium::BLUETOOTH:
          available = mediums_->GetBluetoothClassic().IsAvailable();
          break;
        default:
          break;
      }
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
Medium BwuManager::ChooseBestUpgradeMedium(
    const std::string& endpoint_id, const std::vector<Medium>& mediums) const {
  auto available_mediums = StripOutUnavailableMediums(mediums);
  Medium current_medium = GetBwuMediumForEndpoint(endpoint_id);
  if (current_medium == Medium::UNKNOWN_MEDIUM) {
    if (!available_mediums.empty()) {
      // Case 1: This is our first time upgrading, and we have at least one
      // supported medium to choose from. Return the first medium in the list,
      // since they are ordered by preference.
      return available_mediums[0];
    }
    // Case 2: This is our first time upgrading, but there are no available
    // upgrade mediums. Fall through to returning UNKNOWN_MEDIUM at the
    // bottom.
    NEARBY_LOGS(INFO)
        << "Current upgrade medium is unset, but there are no common supported "
           "upgrade mediums.";
  } else {
    // Case 3: We have already upgraded, and there is a list of supported
    // mediums to check against. Return the current upgrade medium if it's in
    // the supported list.
    if (std::find(available_mediums.begin(), available_mediums.end(),
                  current_medium) != available_mediums.end()) {
      return current_medium;
    }
    // Case 4: We have already upgraded, but the current medium is not
    // supported by the remote endpoint (it's not in the list, or the list is
    // empty). Fall through and return Medium.UNKNOWN_MEDIUM because we cannot
    // continue with the current upgrade medium, and we are not allowed to
    // switch.
    std::string mediums_string;
    for (const auto& medium : available_mediums) {
      absl::StrAppend(&mediums_string,
                      location::nearby::proto::connections::Medium_Name(medium),
                      "; ");
    }
    NEARBY_LOGS(INFO)
        << "Current upgrade medium "
        << location::nearby::proto::connections::Medium_Name(current_medium)
        << " is not supported by the remote endpoint (supported mediums: "
        << mediums_string << ")";
  }

  return Medium::UNKNOWN_MEDIUM;
}

void BwuManager::RetryUpgradesAfterDelay(ClientProxy* client,
                                         const std::string& endpoint_id) {
  absl::Duration delay = CalculateNextRetryDelay(endpoint_id);
  CancelRetryUpgradeAlarm(endpoint_id);
  auto alarm = std::make_unique<CancelableAlarm>(
      "BWU alarm",
      [this, client, endpoint_id]() {
        RunOnBwuManagerThread(
            "bwu-retry-upgrade", [this, client, endpoint_id]() {
              if (!client->IsConnectedToEndpoint(endpoint_id)) {
                return;
              }
              TryNextBestUpgradeMediums(
                  client, endpoint_id,
                  client->GetUpgradeMediums(endpoint_id).GetMediums(true));
            });
      },
      delay, &alarm_executor_);

  retry_upgrade_alarms_.emplace(endpoint_id,
                                std::make_pair(std::move(alarm), delay));
  retry_delays_[endpoint_id] = delay;
  NEARBY_LOGS(INFO) << "Retry bandwidth upgrade after "
                    << absl::FormatDuration(delay);
}

void BwuManager::AttemptToRecordBandwidthUpgradeErrorForUnknownEndpoint(
    location::nearby::proto::connections::BandwidthUpgradeResult result,
    location::nearby::proto::connections::BandwidthUpgradeErrorStage
        error_stage) {
  if (in_progress_upgrades_.size() == 1) {
    auto it = in_progress_upgrades_.begin();
    std::string endpoint_id = it->first;
    ClientProxy* client = it->second;
    // Note: Even though we know this is an error, we cannot clear state yet.
    // We've sent the remote device the credentials they need and it's up to
    // them if they want to repeatedly attempt to connect or if they want to
    // give up and have us try a different medium. This isn't a decision we can
    // make for them.
    client->GetAnalyticsRecorder().OnBandwidthUpgradeError(endpoint_id, result,
                                                           error_stage);
    NEARBY_LOGS(INFO)
        << "BwuManager got error "
        << location::nearby::proto::connections::BandwidthUpgradeResult_Name(
               result)
        << " at stage "
        << location::nearby::proto::connections::
               BandwidthUpgradeErrorStage_Name(error_stage)
        << " when upgrading endpoint " << endpoint_id;
  }
  // Otherwise, we have no way of knowing which endpoint was trying to connect
  // to us :(
  NEARBY_LOGS(INFO)
      << "BwuManager got error "
      << location::nearby::proto::connections::BandwidthUpgradeResult_Name(
             result)
      << " at stage "
      << location::nearby::proto::connections::BandwidthUpgradeErrorStage_Name(
             error_stage)
      << ", but we don't know which endpoint was trying to "
         "connect to us, so skipping analytics for his error.";
}

absl::Duration BwuManager::CalculateNextRetryDelay(
    const std::string& endpoint_id) {
  auto item = retry_delays_.find(endpoint_id);
  auto initial_delay = config_.bandwidth_upgrade_retry_delay;
  if (item == retry_delays_.end()) return initial_delay;
  /*
   * Without use_exp_backoff_in_bwu_retry, bwu retry intervals for the same
   * endpoint_id in seconds will be like: 5, 10, 10, 10...
   * With use_exp_backoff_in_bwu_retry enabled, bwu retry intervals in seconds
   * would be like: 3, 6, 12, 24, ... 300, 300, ...
   */
  auto delay =
      FeatureFlags::GetInstance().GetFlags().use_exp_backoff_in_bwu_retry
          ? item->second * 2
          : item->second + initial_delay;
  return std::min(delay, config_.bandwidth_upgrade_retry_max_delay);
}

void BwuManager::CancelRetryUpgradeAlarm(const std::string& endpoint_id) {
  NEARBY_LOGS(INFO) << "CancelRetryUpgradeAlarm for endpoint " << endpoint_id;
  auto item = retry_upgrade_alarms_.extract(endpoint_id);
  if (item.empty()) return;
  auto& pair = item.mapped();
  pair.first->Cancel();
}

void BwuManager::CancelAllRetryUpgradeAlarms() {
  NEARBY_LOGS(INFO) << "CancelAllRetryUpgradeAlarms invoked";
  for (auto& item : retry_upgrade_alarms_) {
    const std::string& endpoint_id = item.first;
    CancelableAlarm* cancellable_alarm = item.second.first.get();
    NEARBY_LOGS(INFO) << "CancelRetryUpgradeAlarm for endpoint " << endpoint_id;
    cancellable_alarm->Cancel();
  }
  retry_upgrade_alarms_.clear();
  retry_delays_.clear();
}

}  // namespace connections
}  // namespace nearby
