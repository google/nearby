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

#include "connections/implementation/simulation_user.h"
#include <functional>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "connections/listeners.h"
#include "connections/out_of_band_connection_metadata.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "connections/v3/connection_listening_options.h"
#include "internal/interop/device.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/future.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {

void SimulationUser::OnConnectionInitiated(const std::string& endpoint_id,
                                           const ConnectionResponseInfo& info,
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

void SimulationUser::OnConnectionAccepted(const std::string& endpoint_id) {
  NotifyIfNotNotified(accept_notification_, notification_mutex_);
}

void SimulationUser::OnConnectionRejected(const std::string& endpoint_id,
                                          Status status) {
  NotifyIfNotNotified(reject_notification_, notification_mutex_);
}

void SimulationUser::OnEndpointFound(const std::string& endpoint_id,
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

void SimulationUser::OnEndpointLost(const std::string& endpoint_id) {
  NotifyIfNotNotified(lost_notification_, notification_mutex_);
}

void SimulationUser::OnPayload(absl::string_view endpoint_id, Payload payload) {
  payload_ = std::move(payload);
  NotifyIfNotNotified(payload_notification_, notification_mutex_);
}

void SimulationUser::OnPayloadProgress(absl::string_view endpoint_id,
                                       const PayloadProgressInfo& info) {
  absl::MutexLock lock(progress_mutex_);
  progress_info_ = info;
  if (future_ && predicate_ && predicate_(info)) future_->Set(true);
}

bool SimulationUser::WaitForProgress(
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

void SimulationUser::StartAdvertising(const std::string& service_id,
                                      absl::Notification* notification) {
  initiated_notification_ = notification;
  service_id_ = service_id;
  ConnectionListener listener = {
      .initiated_cb =
          std::bind(&SimulationUser::OnConnectionInitiated, this,
                    std::placeholders::_1, std::placeholders::_2, false),
      .accepted_cb =
          absl::bind_front(&SimulationUser::OnConnectionAccepted, this),
      .rejected_cb =
          absl::bind_front(&SimulationUser::OnConnectionRejected, this),
  };
  EXPECT_TRUE(mgr_.StartAdvertising(&client_, service_id_, advertising_options_,
                                    {
                                        .endpoint_info = info_,
                                        .listener = std::move(listener),
                                    })
                  .Ok());
}

void SimulationUser::StartDiscovery(const std::string& service_id,
                                    absl::Notification* notification) {
  found_notification_ = notification;
  EXPECT_TRUE(
      mgr_.StartDiscovery(&client_, service_id, discovery_options_,
                          {
                              .endpoint_found_cb = absl::bind_front(
                                  &SimulationUser::OnEndpointFound, this),
                              .endpoint_lost_cb = absl::bind_front(
                                  &SimulationUser::OnEndpointLost, this),
                          })
          .Ok());
}

void SimulationUser::StopDiscovery() { mgr_.StopDiscovery(&client_); }

Status SimulationUser::UpdateDiscoveryOptions(absl::string_view service_id) {
  return mgr_.UpdateDiscoveryOptions(&client_, service_id, discovery_options_);
}

void SimulationUser::InjectEndpoint(
    const std::string& service_id,
    const OutOfBandConnectionMetadata& metadata) {
  mgr_.InjectEndpoint(&client_, service_id, metadata);
}

void SimulationUser::RequestConnection(absl::Notification* notification) {
  initiated_notification_ = notification;
  ConnectionListener listener = {
      .initiated_cb =
          std::bind(&SimulationUser::OnConnectionInitiated, this,
                    std::placeholders::_1, std::placeholders::_2, true),
      .accepted_cb =
          absl::bind_front(&SimulationUser::OnConnectionAccepted, this),
      .rejected_cb =
          absl::bind_front(&SimulationUser::OnConnectionRejected, this),
  };
  client_.AddCancellationFlag(discovered_.endpoint_id);
  EXPECT_TRUE(
      mgr_.RequestConnection(&client_, discovered_.endpoint_id,
                             {
                                 .endpoint_info = discovered_.endpoint_info,
                                 .listener = std::move(listener),
                             },
                             connection_options_)
          .Ok());
}

void SimulationUser::RequestConnectionV3(absl::Notification* notification,
                                         const NearbyDevice& remote_device) {
  initiated_notification_ = notification;
  ConnectionListener listener = {
      .initiated_cb =
          std::bind(&SimulationUser::OnConnectionInitiated, this,
                    std::placeholders::_1, std::placeholders::_2, true),
      .accepted_cb =
          absl::bind_front(&SimulationUser::OnConnectionAccepted, this),
      .rejected_cb =
          absl::bind_front(&SimulationUser::OnConnectionRejected, this),
  };
  client_.AddCancellationFlag(remote_device.GetEndpointId());
  EXPECT_TRUE(
      mgr_.RequestConnectionV3(&client_, remote_device,
                               {
                                   .endpoint_info = discovered_.endpoint_info,
                                   .listener = std::move(listener),
                               },
                               connection_options_)
          .Ok());
}

void SimulationUser::AcceptConnection(absl::Notification* notification) {
  accept_notification_ = notification;
  PayloadListener listener = {
      .payload_cb = absl::bind_front(&SimulationUser::OnPayload, this),
      .payload_progress_cb =
          absl::bind_front(&SimulationUser::OnPayloadProgress, this),
  };
  EXPECT_TRUE(mgr_.AcceptConnection(&client_, discovered_.endpoint_id,
                                    std::move(listener))
                  .Ok());
}

void SimulationUser::RejectConnection(absl::Notification* notification) {
  reject_notification_ = notification;
  EXPECT_TRUE(mgr_.RejectConnection(&client_, discovered_.endpoint_id).Ok());
}

void SimulationUser::StartListeningForIncomingConnections(
    absl::Notification* notification, absl::string_view service_id,
    const v3::ConnectionListeningOptions& options, Status expected_status) {
  auto result = mgr_.StartListeningForIncomingConnections(
      &client_, service_id, /*listener=*/{}, options);
  NotifyIfNotNotified(notification, notification_mutex_);
  LOG(INFO) << "status: " << result.first.ToString();
  EXPECT_EQ(expected_status, result.first);
}

}  // namespace connections
}  // namespace nearby
