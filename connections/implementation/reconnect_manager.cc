// Copyright 2023 Google LLC
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

#include "connections/implementation/reconnect_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "securegcm/ukey2_handshake.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/bluetooth_endpoint_channel.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/encryption_runner.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/service_id_constants.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {
constexpr absl::string_view TAG = "[ReconnectManager]";

ReconnectManager::ReconnectManager(Mediums& mediums,
                                   EndpointChannelManager& channel_manager)
    : mediums_(&mediums), channel_manager_(&channel_manager) {}

ReconnectManager::~ReconnectManager() { Shutdown(); }

bool ReconnectManager::AutoReconnect(ClientProxy* client,
                                     const std::string& endpoint_id,
                                     AutoReconnectCallback& callback,
                                     bool send_disconnection_notification,
                                     DisconnectionReason disconnection_reason) {
  if (!client->IsAutoReconnectEnabled(endpoint_id)) {
    return false;
  }

  if (resumed_endpoints_.contains(endpoint_id)) {
    LOG(INFO) << TAG << "AutoReconnect is not needed for endpoint_id = "
                      << endpoint_id
                      << ", since it's just reconnected successfully.";
    return true;
  }

  auto endpoint_channel = channel_manager_->GetChannelForEndpoint(endpoint_id);
  if (endpoint_channel == nullptr) {
    LOG(INFO)
        << TAG << " endpoint_channel shouldn't be null for endpoint_id = "
        << endpoint_id;
    return false;
  }
  Medium medium = endpoint_channel->GetMedium();

  bool is_incoming = client->IsIncomingConnection(endpoint_id);
  if (is_incoming == client->IsOutgoingConnection(endpoint_id)) {
    LOG(INFO)
        << TAG << " autoReconnect failed for medium: "
        << location::nearby::proto::connections::Medium_Name(medium)
        << " because there is no existing incoming/outgoing connection, "
           "is_incoming_connection = "
        << is_incoming << ", is_outgoing_connection = "
        << client->IsOutgoingConnection(endpoint_id);
    return false;
  }
  std::string reconnect_service_id =
      WrapInitiatorReconnectServiceId(endpoint_channel->GetServiceId());
  endpoint_id_metadata_map_.emplace(
      endpoint_id,
      ReconnectMetadata(is_incoming, std::move(callback),
                        send_disconnection_notification, disconnection_reason,
                        reconnect_service_id));
  LOG(INFO) << TAG << "add a new endpoint_id " << endpoint_id
                    << " into metadata_by_service_id_map.";

  if (Start(is_incoming, client, endpoint_id, reconnect_service_id, medium)) {
    resumed_endpoints_.emplace(endpoint_id);

    auto time_out =
        FeatureFlags::GetInstance()
            .GetFlags()
            .safe_to_disconnect_reconnect_skip_duplicated_endpoint_duration;
    std::make_unique<CancelableAlarm>(
        absl::StrCat("RemoveSuccessfulResumedEndpointId for ", endpoint_id),
        [this, endpoint_id, time_out]() {
          LOG(INFO)
              << TAG << "Timeout after " << time_out
              << "ms. RemoveSuccessfulResumedEndpointId for " << endpoint_id;
          resumed_endpoints_.erase(endpoint_id);
        },
        time_out, &alarm_executor_);

    return true;
  }

  ClearReconnectData(client, reconnect_service_id, is_incoming);
  return false;
}

bool ReconnectManager::Start(bool is_incoming, ClientProxy* client,
                             const std::string& endpoint_id,
                             const std::string& reconnect_service_id,
                             Medium medium) {
  auto retry_delay_millis =
      FeatureFlags::GetInstance()
          .GetFlags()
          .safe_to_disconnect_reconnect_retry_delay_millis;
  auto reconnect_retry_num = FeatureFlags::GetInstance()
                                 .GetFlags()
                                 .safe_to_disconnect_reconnect_retry_attempts;
  LOG(INFO) << TAG << " " << (is_incoming ? "rehost" : "reconnect")
                    << " for medium: "
                    << location::nearby::proto::connections::Medium_Name(medium)
                    << " for endpoint_id " << endpoint_id << " started...";
  bool final_result = false;
  CountDownLatch latch(1);
  reconnect_executor_.Execute(
      "reconnect-start", [this, &final_result, is_incoming, client, endpoint_id,
                          &reconnect_service_id, retry_delay_millis,
                          reconnect_retry_num, medium, &latch]() mutable {
        for (int i = 0; i < reconnect_retry_num; ++i) {
          if (client->GetCancellationFlag(endpoint_id)->Cancelled()) {
            LOG(INFO)
                << TAG << " Stop retry, Endpoint connection is cancelled";
            break;
          }
          if (RunOnce(is_incoming, client, endpoint_id, reconnect_service_id,
                      medium)) {
            final_result = true;
            break;
          }
          SystemClock::Sleep(retry_delay_millis);
        }
        LOG(INFO) << "Reconnect "
                          << (final_result ? "succeeded" : "failed");
        latch.CountDown();
      });
  latch.Await();
  if (!final_result) {
    client->GetCancellationFlag(endpoint_id)->Cancel();
  }
  return final_result;
}

bool ReconnectManager::RunOnce(bool is_incoming, ClientProxy* client,
                               const std::string& endpoint_id,
                               const std::string& reconnect_service_id,
                               Medium medium) {
  bool result = false;
  switch (medium) {
    case Medium::BLUETOOTH: {
      BluetoothImpl bluetooth_impl(client, endpoint_id, reconnect_service_id,
                                   is_incoming, medium, mediums_,
                                   channel_manager_, *this);
      result = bluetooth_impl.Run();
    } break;

    default:
      LOG(INFO) << "AutoReconnect not implemented yet for "
                        << location::nearby::proto::connections::Medium_Name(
                               medium);

      break;
  }
  return result;
}

void ReconnectManager::ClearReconnectData(
    ClientProxy* client, const std::string& reconnect_service_id,
    bool is_incoming) {
  for (auto& item : endpoint_id_metadata_map_) {
    if (item.second.reconnect_service_id == reconnect_service_id &&
        item.second.is_incoming == is_incoming) {
      if (item.second.reconnect_cb.on_reconnect_failure_cb) {
        item.second.reconnect_cb.on_reconnect_failure_cb(
            client, item.first, item.second.send_disconnection_notification,
            item.second.disconnection_reason);
      }
    }
    LOG(INFO) << TAG << "erase endpoint_id " << item.first;
    endpoint_id_metadata_map_.erase(item.first);
  }
}

void ReconnectManager::Shutdown() {
  LOG(INFO) << TAG << "Initiating shutdown of ReconnectManager.";
  {
    MutexLock lock(&mutex_);
    listen_timeout_alarm_by_service_id_.clear();
  }
  new_endpoint_channels_.clear();
  endpoint_id_metadata_map_.clear();
  resumed_endpoints_.clear();

  alarm_executor_.Shutdown();
  reconnect_executor_.Shutdown();
  encryption_cb_executor_.Shutdown();
  incoming_connection_cb_executor_.Shutdown();
  LOG(INFO) << TAG << "ReconnectManager has shut down.";
}

bool ReconnectManager::BaseMediumImpl::Run() {
  if (!IsMediumRadioOn()) {
    LOG(INFO) << TAG
                      << location::nearby::proto::connections::Medium_Name(
                             medium_)
                      << " radio is turned off, try later";
    return false;
  }

  if (client_->IsConnectedToEndpoint(endpoint_id_)) {
    LOG(INFO) << TAG
                      << "ReconnectBluetooth is not needed since it's already "
                         "connected to the RemoteDevice: ";
    return true;
  }

  auto previou_channel = channel_manager_->GetChannelForEndpoint(endpoint_id_);
  if (previou_channel == nullptr) {
    LOG(INFO)
        << TAG
        << "ReconnectionManager didn't find a previous EndpointChannel "
           "for "
        << endpoint_id_ << " in this run, stop Reconnection!";
    return false;
  }
  previou_channel->Close(
      DisconnectionReason::PREV_CHANNEL_DISCONNECTION_IN_RECONNECT);

  return is_incoming_ ? RehostForIncomingConnections(/*is_last_medium*/ true)
                      : ReconnectToRemoteDevice();
}

bool ReconnectManager::BaseMediumImpl::RehostForIncomingConnections(
    bool is_last_medium) {
  auto time_out = FeatureFlags::GetInstance()
                      .GetFlags()
                      .safe_to_disconnect_reconnect_timeout_millis;
  auto cancellation_flag = client_->GetCancellationFlag(endpoint_id_);

  if (IsListeningForIncomingConnections()) {
    LOG(INFO) << "Rehosting is not needed since it's already "
                         "rehosts for: "
                      << reconnect_service_id_;
    if (cancellation_flag == nullptr) {
      return true;
    }
    if (cancellation_flag->Cancelled()) {
      StopListeningIfAllConnected(
          reconnect_service_id_,
          [this]() { StopListeningForIncomingConnections(); },
          /* forceStop= */ false);
      return false;
    } else {
      auto cancellation_listener =
          std::make_unique<nearby::CancellationFlagListener>(
              cancellation_flag, [this]() {
                LOG(INFO) << "Calling CancellationFlagListener.";
                ProcessFailedReconnection(endpoint_id_, [this]() {
                  StopListeningForIncomingConnections();
                });
              });
      std::make_unique<CancelableAlarm>(
          absl::StrCat(TAG, " unregisterOnCancelListener"),
          [cancellation_listener = std::move(cancellation_listener)]() mutable {
            // clean up the listener after auto reconnect is done.
            cancellation_listener.reset();
          },
          time_out, &reconnect_manager_.alarm_executor_);
    }
    return true;
  }

  LOG(INFO) << "Start rehosting for: " << reconnect_service_id_;
  if (!StartListeningForIncomingConnections()) {
    LOG(ERROR) << TAG
                       << "Rehost failed since "
                          "StartListeningForIncomingConnections return false.";
    return false;
  }
  {
    MutexLock lock(&reconnect_manager_.mutex_);
    reconnect_manager_
        .listen_timeout_alarm_by_service_id_[reconnect_service_id_] =
        std::make_unique<CancelableAlarm>(
            absl::StrCat("Rehost listen timeout for ", reconnect_service_id_),
            [this, time_out, is_last_medium]() {
              LOG(INFO)
                  << "Timeout after " << time_out
                  << "ms. Stop listening for incoming "
                     "Connections for serviceId "
                  << reconnect_service_id_ << " for rehost, initiated by "
                  << endpoint_id_
                  << ", unregister all still not connected endpointIds.";
              StopListeningIfAllConnected(
                  reconnect_service_id_,
                  [this]() { StopListeningForIncomingConnections(); },
                  /* forceStop= */ is_last_medium);
            },
            time_out, &reconnect_manager_.alarm_executor_);
  }

  if (cancellation_flag == nullptr) {
    return true;
  }
  if (cancellation_flag->Cancelled()) {
    StopListeningIfAllConnected(
        reconnect_service_id_,
        [this]() { StopListeningForIncomingConnections(); },
        /* forceStop= */ false);
    return false;
  }
  auto cancellation_listener =
      std::make_unique<nearby::CancellationFlagListener>(
          cancellation_flag, [this]() {
            LOG(INFO) << "Calling CancellationFlagListener.";
            ProcessFailedReconnection(endpoint_id_, [this]() {
              StopListeningForIncomingConnections();
            });
          });

  std::make_unique<CancelableAlarm>(
      absl::StrCat(TAG, " unregisterOnCancelListener"),
      [cancellation_listener = std::move(cancellation_listener)]() mutable {
        // clean up the listener after auto reconnect is done.
        cancellation_listener.reset();
      },
      time_out, &reconnect_manager_.alarm_executor_);
  return true;
}

bool ReconnectManager::BaseMediumImpl::ReconnectToRemoteDevice() {
  if (!ConnectOverMedium()) {
    LOG(INFO) << TAG << "Connect over medium "
                      << location::nearby::proto::connections::Medium_Name(
                             medium_)
                      << " failed.";
    return false;
  }
  LOG(INFO) << TAG << "Write CLIENT_INTRODUCTION frame";
  Exception write_exception = reconnect_channel_->Write(
      parser::ForAutoReconnectIntroduction(client_->GetLocalEndpointId()));
  if (!write_exception.Ok()) {
    LOG(ERROR)
        << TAG << "Failed to write forAutoReconnectClientIntroductionEvent.";
    QuietlyCloseChannelAndSocket();
    return false;
  }
  if (!ReadClientIntroductionAckFrame(reconnect_channel_.get())) {
    LOG(ERROR) << TAG << "Failed to read ClientIntroductionAck frame.";
    QuietlyCloseChannelAndSocket();
    return false;
  }
  if (ReplaceChannelForEndpoint(client_, endpoint_id_,
                                std::move(reconnect_channel_),
                                SupportEncryptionDisabled(), nullptr)) {
    LOG(INFO) << TAG
                      << " successfully rebuild the outgoing connection with "
                      << location::nearby::proto::connections::Medium_Name(
                             medium_)
                      << " for the endpointId:" << endpoint_id_;
    return true;
  }
  LOG(INFO)
      << TAG << " ReplaceChannelForEndpoint for the outgoing connection with "
      << location::nearby::proto::connections::Medium_Name(medium_)
      << " for the endpointId:" << endpoint_id_ << " failed. Please retry";
  return false;
}

void ReconnectManager::BaseMediumImpl::OnIncomingConnection(
    const std::string& reconnect_service_id) {
  LOG(INFO) << TAG << "Received reconnection successfully";
  reconnect_manager_.incoming_connection_cb_executor_.Execute(
      "OnIncomingConnection", [this]() {
        auto incoming_endpoint_id =
            ReadClientIntroductionFrame(reconnect_channel_.get());
        if (incoming_endpoint_id.empty()) {
          LOG(ERROR) << TAG << "read ClientIntroductionFrame failed";
          QuietlyCloseChannelAndSocket();
          return;
        }
        Exception write_exception = reconnect_channel_->Write(
            parser::ForAutoReconnectIntroductionAck());
        if (!write_exception.Ok()) {
          LOG(ERROR)
              << TAG
              << "Failed to write forAutoReconnectClientIntroductionAckEvent.";
          QuietlyCloseChannelAndSocket();
          return;
        }
        LOG(INFO)
            << TAG << "successfully read ClientIntroductionFrame and write"
                      " ClientIntroductionAckFrame with"
            << location::nearby::proto::connections::Medium_Name(medium_)
            << " for the incoming endpointId " << incoming_endpoint_id;
        if (ReplaceChannelForEndpoint(
                client_, incoming_endpoint_id, std::move(reconnect_channel_),
                SupportEncryptionDisabled(),
                [this]() { StopListeningForIncomingConnections(); })) {
          LOG(INFO)
              << TAG << " successfully rebuild the incoming connection with "
              << location::nearby::proto::connections::Medium_Name(medium_)
              << " for the endpointId:" << incoming_endpoint_id;
          return;
        }
        QuietlyCloseChannelAndSocket();
        LOG(INFO)
            << TAG
            << " ReplaceChannelForEndpoint for the incoming connection with "
            << location::nearby::proto::connections::Medium_Name(medium_)
            << " for the endpointId:" << incoming_endpoint_id
            << " failed. Please retry";
        return;
      });
}

std::string ReconnectManager::BaseMediumImpl::ReadClientIntroductionFrame(
    EndpointChannel* endpoint_channel) {
  LOG(INFO) << TAG << "Read CLIENT_INTRODUCTION frame";

  auto timeout = FeatureFlags::GetInstance()
                     .GetFlags()
                     .safe_to_disconnect_auto_resume_timeout_millis;
  CancelableAlarm timeout_alarm(
      "ReconnectManager::ReadClientIntroductionFrame",
      [timeout, endpoint_channel]() {
        LOG(ERROR) << "In ReconnectManager, failed to read the "
                              "ClientIntroductionFrame after "
                           << timeout
                           << ". Timing out and closing EndpointChannel "
                           << endpoint_channel->GetType();
        endpoint_channel->Close();
      },
      timeout, &reconnect_manager_.alarm_executor_);

  auto data = endpoint_channel->Read();
  timeout_alarm.Cancel();
  if (!data.ok()) {
    LOG(ERROR)
        << "Data read fail when expecting a ClientIntroductionFrame from "
           "EndpointChannel "
        << endpoint_channel->GetType();
    return {};
  }
  auto transfer(parser::FromBytes(data.result()));
  if (!transfer.ok()) {
    LOG(ERROR) << "Attempted to read a ClientIntroductionFrame from "
                          "EndpointChannel "
                       << endpoint_channel->GetType()
                       << ", but was unable to obtain any OfflineFrame.";
    return {};
  }
  OfflineFrame frame = transfer.result();
  if (!frame.has_v1() || !frame.v1().has_auto_reconnect()) {
    LOG(ERROR) << "In ReadClientIntroductionFrame(), eExpected a "
                          "AUTO_RECONNECT v1 OfflineFrame but got a "
                       << parser::GetFrameType(frame) << " frame instead.";
    return {};
  }
  if (frame.v1().auto_reconnect().event_type() !=
      AutoReconnectFrame::CLIENT_INTRODUCTION) {
    LOG(ERROR) << "In ReadClientIntroductionFrame(), expected a "
                          "CLIENT_INTRODUCTION "
                          "v1 OfflineFrame but got a AUTO_RECONNECT frame "
                          "with eventType "
                       << frame.v1().auto_reconnect().event_type()
                       << " instead.";
    return {};
  }
  return frame.v1().auto_reconnect().endpoint_id();
}

bool ReconnectManager::BaseMediumImpl::ReadClientIntroductionAckFrame(
    EndpointChannel* endpoint_channel) {
  LOG(INFO) << TAG << "Read CLIENT_INTRODUCTION_ACK frame";

  auto timeout = FeatureFlags::GetInstance()
                     .GetFlags()
                     .safe_to_disconnect_auto_resume_timeout_millis;
  CancelableAlarm timeout_alarm(
      "ReconnectManager::ReadClientIntroductionAckFrame",
      [timeout, endpoint_channel]() {
        LOG(ERROR) << "In ReconnectManager, failed to read the "
                              "ClientIntroductionAckFrame after "
                           << timeout
                           << ". Timing out and closing EndpointChannel "
                           << endpoint_channel->GetType();
        endpoint_channel->Close();
      },
      timeout, &reconnect_manager_.alarm_executor_);

  auto data = endpoint_channel->Read();
  timeout_alarm.Cancel();
  if (!data.ok()) return false;
  auto transfer(parser::FromBytes(data.result()));
  if (!transfer.ok()) {
    LOG(ERROR) << "Attempted to read a ClientIntroductionAckFrame from "
                          "EndpointChannel "
                       << endpoint_channel->GetType()
                       << ", but was unable to obtain any OfflineFrame.";
    return false;
  }
  OfflineFrame frame = transfer.result();
  if (!frame.has_v1() || !frame.v1().has_auto_reconnect()) {
    LOG(ERROR) << "In ReadClientIntroductionAckFrame(), eExpected a "
                          "AUTO_RECONNECT v1 OfflineFrame but got a "
                       << parser::GetFrameType(frame) << " frame instead.";
    return false;
  }
  if (frame.v1().auto_reconnect().event_type() !=
      AutoReconnectFrame::CLIENT_INTRODUCTION_ACK) {
    LOG(ERROR) << "In ReadClientIntroductionAckFrame(), expected a "
                          "CLIENT_INTRODUCTION_ACK "
                          "v1 OfflineFrame but got a AUTO_RECONNECT frame "
                          "with eventType "
                       << frame.v1().auto_reconnect().event_type()
                       << " instead.";
    return false;
  }
  return true;
}

bool ReconnectManager::BaseMediumImpl::ReplaceChannelForEndpoint(
    ClientProxy* client, const std::string& endpoint_id,
    std::unique_ptr<EndpointChannel> new_channel,
    bool support_encryption_disabled,
    absl::AnyInvocable<void(void)> stop_listening_incoming_connection) {
  auto& endpoint_id_metadata_map = reconnect_manager_.endpoint_id_metadata_map_;
  auto reconnect_metadata = endpoint_id_metadata_map.find(endpoint_id);
  if (reconnect_metadata == endpoint_id_metadata_map.end()) {
    LOG(ERROR) << TAG << "ReconnectMetadata is null for endpointId: "
                       << endpoint_id << " ,please retry!";
    return false;
  }

  EndpointChannel* endpoint_channel =
      reconnect_manager_.new_endpoint_channels_
          .emplace(endpoint_id, std::move(new_channel))
          .first->second.get();
  {
    MutexLock lock(&mutex_);
    replace_channel_succeed_ = false;
    wait_encryption_to_finish_ = std::make_unique<CountDownLatch>(1);
    if (reconnect_metadata->second.is_incoming) {
      reconnect_manager_.encryption_runner_.StartServer(
          client, endpoint_id, endpoint_channel, GetResultListener());
    } else {
      reconnect_manager_.encryption_runner_.StartClient(
          client, endpoint_id, endpoint_channel, GetResultListener());
    }
    auto cancellation_flag = client_->GetCancellationFlag(endpoint_id_);
    std::unique_ptr<nearby::CancellationFlagListener> cancellation_listener;
    if (cancellation_flag != nullptr) {
      cancellation_listener =
          std::make_unique<nearby::CancellationFlagListener>(
              cancellation_flag, [this]() {
                LOG(INFO) << "Calling CancellationFlagListener for "
                                     "stopping wait_encryption_to_finish";
                MutexLock lock(&mutex_);
                replace_channel_succeed_ = false;
                wait_encryption_to_finish_->CountDown();
              });
    }

    wait_encryption_to_finish_->Await(
        FeatureFlags::GetInstance()
            .GetFlags()
            .safe_to_disconnect_reconnect_timeout_millis);

    LOG(INFO) << TAG << "replace_channel_succeed_: "
                      << replace_channel_succeed_
                      << " for endpointId: " << endpoint_id;

    if (replace_channel_succeed_) {
      ProcessSuccessfulReconnection(
          endpoint_id, [this]() { StopListeningForIncomingConnections(); });
      client->GetAnalyticsRecorder().OnConnectionEstablished(
          endpoint_id, endpoint_channel->GetMedium(),
          client->GetConnectionToken(endpoint_id));
    } else {
      ProcessFailedReconnection(
          endpoint_id, [this]() { StopListeningForIncomingConnections(); });
    }
    cancellation_listener.reset();
    reconnect_manager_.new_endpoint_channels_.erase(endpoint_id);
    return replace_channel_succeed_;
  }
}

EncryptionRunner::ResultListener
ReconnectManager::BaseMediumImpl::GetResultListener() {
  return {
      .on_success_cb =
          [this](const std::string& endpoint_id,
                 std::unique_ptr<securegcm::UKey2Handshake> ukey2,
                 const std::string& auth_token,
                 const ByteArray& raw_auth_token) {
            reconnect_manager_.encryption_cb_executor_.Execute(
                "encryption-success",
                [this, endpoint_id, raw_ukey2 = ukey2.release(), auth_token,
                 raw_auth_token]() mutable {
                  OnEncryptionSuccessRunnable(
                      endpoint_id,
                      std::unique_ptr<securegcm::UKey2Handshake>(raw_ukey2),
                      auth_token, raw_auth_token);
                  wait_encryption_to_finish_->CountDown();
                });
          },
      .on_failure_cb =
          [this](const std::string& endpoint_id, EndpointChannel* channel) {
            reconnect_manager_.encryption_cb_executor_.Execute(
                "encryption-failure", [this, endpoint_id, channel]() mutable {
                  LOG(ERROR)
                      << "Encryption failed for endpoint_id=" << endpoint_id
                      << " on medium="
                      << location::nearby::proto::connections::Medium_Name(
                             channel->GetMedium());
                  OnEncryptionFailureRunnable(endpoint_id, channel);
                  wait_encryption_to_finish_->CountDown();
                });
          },
  };
}

void ReconnectManager::BaseMediumImpl::OnEncryptionSuccessRunnable(
    const std::string& endpoint_id,
    std::unique_ptr<securegcm::UKey2Handshake> ukey2,
    const std::string& auth_token, const ByteArray& raw_auth_token) {
  auto item = reconnect_manager_.new_endpoint_channels_.find(endpoint_id);
  if (item == reconnect_manager_.new_endpoint_channels_.end()) {
    LOG(INFO) << "TAG"
                      << "OnEncryptionSuccess failed, new_endpoint_channel is "
                         "null for Endpoint:"
                      << endpoint_id;
    return;
  }
  if (!ukey2) {
    LOG(INFO)
        << "TAG"
        << "OnEncryptionSuccess failed, ukey2 is null for Endpoint:"
        << endpoint_id;
    return;
  }

  // After both parties accepted connection (presumably after verifying &
  // matching security tokens), we are allowed to extract the shared key.
  bool succeeded = ukey2->VerifyHandshake();
  CHECK(succeeded);  // If this fails, it's a UKEY2 protocol bug.
  auto context = ukey2->ToConnectionContext();
  CHECK(context);  // there is no way how this can fail, if Verify succeeded.
  // If it did, it's a UKEY2 protocol bug.

  if (!reconnect_manager_.channel_manager_->EncryptChannelForEndpoint(
          endpoint_id, std::move(context))) {
    LOG(INFO) << "TAG"
                      << "new_endpoint_channel failed to update "
                         "EncryptionContext for Endpoint:"
                      << endpoint_id;
    return;
  }
  auto previous_channel =
      reconnect_manager_.channel_manager_->GetChannelForEndpoint(endpoint_id);
  if (previous_channel == nullptr) {
    LOG(INFO)
        << "TAG"
        << "ReconnectionManager didn't find a previous EndpointChannel for "
        << endpoint_id
        << " when registering the new EndpointChannel, stop Reconnection!";
    item->second->Close(DisconnectionReason::UNFINISHED);
    return;
  }
  reconnect_manager_.channel_manager_->ReplaceChannelForEndpoint(
      client_, endpoint_id, std::move(item->second),
      SupportEncryptionDisabled());
  {
    MutexLock lock(&mutex_);
    replace_channel_succeed_ = true;
  }
}

void ReconnectManager::BaseMediumImpl::OnEncryptionFailureRunnable(
    const std::string& endpoint_id, EndpointChannel* endpoint_channel) {
  LOG(INFO)
      << "TAG"
      << "new_endpoint_channel failed to use encryption for Endpoint:"
      << endpoint_id;
}

void ReconnectManager::BaseMediumImpl::ProcessSuccessfulReconnection(
    const std::string& endpoint_id,
    absl::AnyInvocable<void(void)> stop_listening_incoming_connection) {
  auto& endpoint_id_metadata_map = reconnect_manager_.endpoint_id_metadata_map_;
  auto reconnect_metadata = endpoint_id_metadata_map.find(endpoint_id);
  if (reconnect_metadata == endpoint_id_metadata_map.end()) {
    LOG(ERROR) << TAG
                       << "when ProcessSuccessfulReconnection, endpoint_id: "
                       << endpoint_id
                       << " is already removed fromendpoint_id_metadata_map.";
    return;
  }

  auto medatdata = std::move(reconnect_metadata->second);
  endpoint_id_metadata_map.erase(reconnect_metadata);
  auto& callback = medatdata.reconnect_cb;
  if (callback.on_reconnect_success_cb) {
    callback.on_reconnect_success_cb(client_, endpoint_id);
  } else {
    LOG(ERROR) << TAG
                       << "when ProcessSuccessfulReconnection, endpoint_id: "
                       << endpoint_id
                       << " callback.on_reconnect_success_cb is null";
  }

  if (medatdata.is_incoming && stop_listening_incoming_connection) {
    StopListeningIfAllConnected(medatdata.reconnect_service_id,
                                std::move(stop_listening_incoming_connection),
                                /* forceStop= */ false);
  }
}
void ReconnectManager::BaseMediumImpl::ProcessFailedReconnection(
    const std::string& endpoint_id,
    absl::AnyInvocable<void(void)> stop_listening_incoming_connection) {
  auto& endpoint_id_metadata_map = reconnect_manager_.endpoint_id_metadata_map_;
  auto reconnect_metadata = endpoint_id_metadata_map.find(endpoint_id);
  if (reconnect_metadata == endpoint_id_metadata_map.end()) {
    LOG(ERROR) << TAG << "when ProcessFailedReconnection, endpoint_id: "
                       << endpoint_id
                       << " is already removed fromendpoint_id_metadata_map.";
    return;
  }

  auto medatdata = std::move(reconnect_metadata->second);
  auto& callback = medatdata.reconnect_cb;

  if (medatdata.is_incoming) {
    endpoint_id_metadata_map.erase(reconnect_metadata);
    if (callback.on_reconnect_failure_cb) {
      callback.on_reconnect_failure_cb(
          client_, endpoint_id, medatdata.send_disconnection_notification,
          medatdata.disconnection_reason);
    } else {
      LOG(ERROR) << TAG
                         << "when ProcessFailedReconnection, endpoint_id: "
                         << endpoint_id
                         << " callback.on_reconnect_success_cb is null";
    }

    if (stop_listening_incoming_connection) {
      StopListeningIfAllConnected(medatdata.reconnect_service_id,
                                  std::move(stop_listening_incoming_connection),
                                  /* forceStop= */ false);
    }
  }
}

void ReconnectManager::BaseMediumImpl::StopListeningIfAllConnected(
    const std::string& reconnect_service_id,
    absl::AnyInvocable<void(void)> stop_listening_incoming_connection,
    bool force_stop) {
  if (!force_stop && HasPendingIncomingConnections(reconnect_service_id)) {
    return;
  }
  CancelClearHostTimeoutAlarm(reconnect_service_id);
  stop_listening_incoming_connection();
  ClearReconnectData(reconnect_service_id, /* is_incoming= */ true);
  LOG(INFO) << TAG
                    << " No more pending incoming connections, "
                       "stop_listening_incoming_connection for "
                    << reconnect_service_id << " before timeout.";
}

bool ReconnectManager::BaseMediumImpl::HasPendingIncomingConnections(
    const std::string& reconnect_service_id) {
  for (auto& item : reconnect_manager_.endpoint_id_metadata_map_) {
    if (item.second.reconnect_service_id == reconnect_service_id &&
        item.second.is_incoming) {
      return true;
    }
  }
  return false;
}

void ReconnectManager::BaseMediumImpl::CancelClearHostTimeoutAlarm(
    const std::string& service_id) {
  MutexLock lock(&reconnect_manager_.mutex_);
  auto item =
      reconnect_manager_.listen_timeout_alarm_by_service_id_.find(service_id);
  if (item == reconnect_manager_.listen_timeout_alarm_by_service_id_.end())
    return;

  if (item->second->IsValid()) {
    item->second->Cancel();
    item->second.reset();
  }
  reconnect_manager_.listen_timeout_alarm_by_service_id_.erase(item);
}

void ReconnectManager::BaseMediumImpl::ClearReconnectData(
    const std::string& service_id, bool is_incoming) {
  absl::flat_hash_set<std::string> endpoint_ids_pending_removal;
  auto& metadata_map = reconnect_manager_.endpoint_id_metadata_map_;
  for (auto item = metadata_map.begin(); item != metadata_map.end();) {
    if (item->second.reconnect_service_id == service_id && is_incoming) {
      auto& callback = item->second.reconnect_cb.on_reconnect_failure_cb;
      if (callback)
        callback(client_, item->first,
                 item->second.send_disconnection_notification,
                 item->second.disconnection_reason);
      endpoint_ids_pending_removal.insert(item->first);
    } else {
      ++item;
    }
  }

  for (const auto& endpoint_id : endpoint_ids_pending_removal) {
    metadata_map.erase(endpoint_id);
  }
}

bool ReconnectManager::BluetoothImpl::IsMediumRadioOn() const {
  return bluetooth_medium_.IsAvailable();
}

bool ReconnectManager::BluetoothImpl::IsListeningForIncomingConnections()
    const {
  return bluetooth_medium_.IsAcceptingConnections(reconnect_service_id_);
}

bool ReconnectManager::BluetoothImpl::StartListeningForIncomingConnections() {
  if (!bluetooth_medium_.StartAcceptingConnections(
          reconnect_service_id_,
          absl::bind_front(
              &ReconnectManager::BluetoothImpl::OnIncomingBluetoothConnection,
              this, client_))) {
    LOG(ERROR)
        << "ReconnectManager::BluetoothImpl couldn't initiate the "
           "BLUETOOTH reconnect for endpoint "
        << endpoint_id_
        << " because it failed to start listening for "
           "incoming Bluetooth connections.";
    return false;
  }
  LOG(INFO) << "ReconnectManager::BluetoothImpl successfully started "
                       "listening for incoming "
                       "reconnection on service_id="
                    << reconnect_service_id_ << " for endpoint "
                    << endpoint_id_;
  return true;
}

void ReconnectManager::BluetoothImpl::OnIncomingBluetoothConnection(
    ClientProxy* client, const std::string& upgrade_service_id,
    BluetoothSocket socket) {
  reconnect_channel_ = std::make_unique<BluetoothEndpointChannel>(
      upgrade_service_id, /*channel_name=*/upgrade_service_id, socket);
  if (reconnect_channel_ == nullptr) {
    LOG(ERROR) << TAG
                       << "Create new endpointChannel for incoming socket "
                          "failed, close the socket";

    socket.Close();
    return;
  }
  bluetooth_socket_ = std::move(socket);

  LOG(INFO)
      << TAG << "Create new endpointChannel successfully for incoming socket.";

  OnIncomingConnection(upgrade_service_id);
}

void ReconnectManager::BluetoothImpl::StopListeningForIncomingConnections() {
  bluetooth_medium_.StopAcceptingConnections(reconnect_service_id_);
}

bool ReconnectManager::BluetoothImpl::ConnectOverMedium() {
  std::optional<std::string> remote_mac_address =
      client_->GetBluetoothMacAddress(endpoint_id_);
  if (!remote_mac_address.has_value()) {
    LOG(INFO)
        << "ReconnectBluetooth failed since remoteMacAddress is empty";
    return false;
  }
  auto& bluetooth_medium = mediums_->GetBluetoothClassic();
  BluetoothDevice remote_bluetooth_device =
      bluetooth_medium.GetRemoteDevice(remote_mac_address.value());
  if (!remote_bluetooth_device.IsValid()) {
    LOG(INFO)
        << "ReconnectBluetooth failed since remoteBluetoothDevice is null: "
        << remote_mac_address.value();
    return false;
  }

  bluetooth_socket_ =
      bluetooth_medium.Connect(remote_bluetooth_device, reconnect_service_id_,
                               client_->GetCancellationFlag(endpoint_id_));

  if (!bluetooth_socket_.IsValid()) {
    LOG(ERROR) << "Failed to reconnect to Bluetooth device "
                       << remote_bluetooth_device.GetName()
                       << " for endpoint(id=" << endpoint_id_ << ").";
    return false;
  }

  reconnect_channel_ = std::make_unique<BluetoothEndpointChannel>(
      UnWrapInitiatorReconnectServiceId(reconnect_service_id_),
      /*channel_name=*/endpoint_id_, bluetooth_socket_);
  if (reconnect_channel_ == nullptr) {
    LOG(ERROR) << "ReconnectBluetooth Failed to get the Bluetooth "
                          "channel, please retry ";
    bluetooth_socket_.Close();
    return false;
  }
  return true;
}

bool ReconnectManager::BluetoothImpl::SupportEncryptionDisabled() {
  return false;
}

void ReconnectManager::BluetoothImpl::QuietlyCloseChannelAndSocket() {
  reconnect_channel_->Close(DisconnectionReason::UNFINISHED);
  bluetooth_socket_.Close();
}

}  // namespace connections
}  // namespace nearby
