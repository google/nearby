// Copyright 2021 Google LLC
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

#include "connections/implementation/offline_simulation_user.h"
#include <functional>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "connections/advertising_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/simulation_user.h"
#include "connections/listeners.h"
#include "connections/out_of_band_connection_metadata.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "internal/interop/device.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/future.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {

void OfflineSimulationUser::OnConnectionInitiated(
    const std::string& endpoint_id, const ConnectionResponseInfo& info,
    bool is_outgoing) {
  if (is_outgoing) {
    LOG(INFO) << "RequestConnection: initiated_cb called";
  } else {
    LOG(INFO) << "StartAdvertising: initiated_cb called";
    discovered_ = DiscoveredInfo{
        .endpoint_id = endpoint_id,
        .endpoint_info = GetInfo(),
        .service_id = service_id_,
    };
  }
  NotifyIfNotNotified(initiated_notification_, notification_mutex_);
}

void OfflineSimulationUser::OnConnectionAccepted(
    const std::string& endpoint_id) {
  NotifyIfNotNotified(accept_notification_, notification_mutex_);
}

void OfflineSimulationUser::OnConnectionRejected(const std::string& endpoint_id,
                                                 Status status) {
  NotifyIfNotNotified(reject_notification_, notification_mutex_);
}

void OfflineSimulationUser::OnEndpointDisconnect(
    const std::string& endpoint_id) {
  LOG(INFO) << "OnEndpointDisconnect: self=" << this << "; id=" << endpoint_id;
  NotifyIfNotNotified(disconnect_notification_, notification_mutex_);
}

void OfflineSimulationUser::OnEndpointFound(const std::string& endpoint_id,
                                            const ByteArray& endpoint_info,
                                            const std::string& service_id) {
  LOG(INFO) << "Device discovered: id=" << endpoint_id;
  discovered_ = DiscoveredInfo{
      .endpoint_id = endpoint_id,
      .endpoint_info = endpoint_info,
      .service_id = service_id,
  };
  NotifyIfNotNotified(found_notification_, notification_mutex_);
}

void OfflineSimulationUser::OnEndpointLost(const std::string& endpoint_id) {
  NotifyIfNotNotified(lost_notification_, notification_mutex_);
}

void OfflineSimulationUser::OnPayload(absl::string_view endpoint_id,
                                      Payload payload) {
  payload_ = std::move(payload);
  NotifyIfNotNotified(payload_notification_, notification_mutex_);
}

void OfflineSimulationUser::OnPayloadProgress(absl::string_view endpoint_id,
                                              const PayloadProgressInfo& info) {
  absl::MutexLock lock(progress_mutex_);
  progress_info_ = info;
  if (future_ && predicate_ && predicate_(info)) future_->Set(true);
}

bool OfflineSimulationUser::WaitForProgress(
    absl::AnyInvocable<bool(const PayloadProgressInfo&)> predicate,
    absl::Duration timeout) {
  Future<bool> future;
  {
    absl::MutexLock lock(progress_mutex_);
    if (predicate(progress_info_)) return true;
    future_ = &future;
    predicate_ = std::move(predicate);
  }
  auto response = future.Get(timeout);
  {
    absl::MutexLock lock(progress_mutex_);
    future_ = nullptr;
    predicate_ = nullptr;
  }
  return response.ok() && response.result();
}

Status OfflineSimulationUser::StartAdvertising(
    const std::string& service_id, absl::Notification* notification) {
  initiated_notification_ = notification;
  service_id_ = service_id;
  ConnectionListener listener = {
      .initiated_cb =
          std::bind(&OfflineSimulationUser::OnConnectionInitiated, this,
                    std::placeholders::_1, std::placeholders::_2, false),
      .accepted_cb =
          absl::bind_front(&OfflineSimulationUser::OnConnectionAccepted, this),
      .rejected_cb =
          absl::bind_front(&OfflineSimulationUser::OnConnectionRejected, this),
      .disconnected_cb =
          absl::bind_front(&OfflineSimulationUser::OnEndpointDisconnect, this),
  };
  return ctrl_.StartAdvertising(&client_, service_id_, advertising_options_,
                                {
                                    .endpoint_info = info_,
                                    .listener = std::move(listener),
                                });
}

void OfflineSimulationUser::StopAdvertising() {
  ctrl_.StopAdvertising(&client_);
}

Status OfflineSimulationUser::UpdateAdvertisingOptions(
    absl::string_view service_id, const AdvertisingOptions& options) {
  return ctrl_.UpdateAdvertisingOptions(&client_, service_id, options);
}

Status OfflineSimulationUser::StartDiscovery(
    const std::string& service_id, absl::Notification* found_notification,
    absl::Notification* lost_notification) {
  found_notification_ = found_notification;
  lost_notification_ = lost_notification;
  DiscoveryListener listener = {
      .endpoint_found_cb =
          absl::bind_front(&OfflineSimulationUser::OnEndpointFound, this),
      .endpoint_lost_cb =
          absl::bind_front(&OfflineSimulationUser::OnEndpointLost, this),
  };
  return ctrl_.StartDiscovery(&client_, service_id, discovery_options_,
                              std::move(listener));
}

void OfflineSimulationUser::StopDiscovery() { ctrl_.StopDiscovery(&client_); }

Status OfflineSimulationUser::UpdateDiscoveryOptions(
    absl::string_view service_id, const DiscoveryOptions& options) {
  return ctrl_.UpdateDiscoveryOptions(&client_, service_id, options);
}

void OfflineSimulationUser::InjectEndpoint(
    const std::string& service_id,
    const OutOfBandConnectionMetadata& metadata) {
  ctrl_.InjectEndpoint(&client_, service_id, metadata);
}

Status OfflineSimulationUser::RequestConnection(
    absl::Notification* notification) {
  initiated_notification_ = notification;
  ConnectionListener listener = {
      .initiated_cb =
          std::bind(&OfflineSimulationUser::OnConnectionInitiated, this,
                    std::placeholders::_1, std::placeholders::_2, true),
      .accepted_cb =
          absl::bind_front(&OfflineSimulationUser::OnConnectionAccepted, this),
      .rejected_cb =
          absl::bind_front(&OfflineSimulationUser::OnConnectionRejected, this),
      .disconnected_cb =
          absl::bind_front(&OfflineSimulationUser::OnEndpointDisconnect, this),
  };
  client_.AddCancellationFlag(discovered_.endpoint_id);
  return ctrl_.RequestConnection(&client_, discovered_.endpoint_id,
                                 {
                                     .endpoint_info = discovered_.endpoint_info,
                                     .listener = std::move(listener),
                                 },
                                 connection_options_);
}

Status OfflineSimulationUser::RequestConnectionV3(
    absl::Notification* notification, const NearbyDevice& remote_device) {
  initiated_notification_ = notification;
  ConnectionListener listener = {
      .initiated_cb =
          std::bind(&OfflineSimulationUser::OnConnectionInitiated, this,
                    std::placeholders::_1, std::placeholders::_2, true),
      .accepted_cb =
          absl::bind_front(&OfflineSimulationUser::OnConnectionAccepted, this),
      .rejected_cb =
          absl::bind_front(&OfflineSimulationUser::OnConnectionRejected, this),
      .disconnected_cb =
          absl::bind_front(&OfflineSimulationUser::OnEndpointDisconnect, this),
  };
  client_.AddCancellationFlag(remote_device.GetEndpointId());
  return ctrl_.RequestConnectionV3(
      &client_, remote_device,
      {
          .endpoint_info = discovered_.endpoint_info,
          .listener = std::move(listener),
      },
      connection_options_);
}

Status OfflineSimulationUser::AcceptConnection(
    absl::Notification* notification) {
  accept_notification_ = notification;
  PayloadListener listener = {
      .payload_cb = absl::bind_front(&OfflineSimulationUser::OnPayload, this),
      .payload_progress_cb =
          absl::bind_front(&OfflineSimulationUser::OnPayloadProgress, this),
  };
  return ctrl_.AcceptConnection(&client_, discovered_.endpoint_id,
                                std::move(listener));
}

Status OfflineSimulationUser::RejectConnection(
    absl::Notification* notification) {
  reject_notification_ = notification;
  return ctrl_.RejectConnection(&client_, discovered_.endpoint_id);
}

void OfflineSimulationUser::Disconnect() {
  LOG(INFO) << "Disconnecting from id=" << discovered_.endpoint_id;
  ctrl_.DisconnectFromEndpoint(&client_, discovered_.endpoint_id);
}

}  // namespace connections
}  // namespace nearby
