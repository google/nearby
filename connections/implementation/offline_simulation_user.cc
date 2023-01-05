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

#include "absl/functional/bind_front.h"
#include "connections/listeners.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/system_clock.h"

namespace nearby {
namespace connections {

void OfflineSimulationUser::OnConnectionInitiated(
    const std::string& endpoint_id, const ConnectionResponseInfo& info,
    bool is_outgoing) {
  if (is_outgoing) {
    NEARBY_LOG(INFO, "RequestConnection: initiated_cb called");
  } else {
    NEARBY_LOG(INFO, "StartAdvertising: initiated_cb called");
    discovered_ = DiscoveredInfo{
        .endpoint_id = endpoint_id,
        .endpoint_info = GetInfo(),
        .service_id = service_id_,
    };
  }
  if (initiated_latch_) initiated_latch_->CountDown();
}

void OfflineSimulationUser::OnConnectionAccepted(
    const std::string& endpoint_id) {
  if (accept_latch_) accept_latch_->CountDown();
}

void OfflineSimulationUser::OnConnectionRejected(const std::string& endpoint_id,
                                                 Status status) {
  if (reject_latch_) reject_latch_->CountDown();
}

void OfflineSimulationUser::OnEndpointDisconnect(
    const std::string& endpoint_id) {
  NEARBY_LOGS(INFO) << "OnEndpointDisconnect: self=" << this
                    << "; id=" << endpoint_id;
  if (disconnect_latch_) disconnect_latch_->CountDown();
}

void OfflineSimulationUser::OnEndpointFound(const std::string& endpoint_id,
                                            const ByteArray& endpoint_info,
                                            const std::string& service_id) {
  NEARBY_LOG(INFO, "Device discovered: id=%s", endpoint_id.c_str());
  discovered_ = DiscoveredInfo{
      .endpoint_id = endpoint_id,
      .endpoint_info = endpoint_info,
      .service_id = service_id,
  };
  if (found_latch_) found_latch_->CountDown();
}

void OfflineSimulationUser::OnEndpointLost(const std::string& endpoint_id) {
  if (lost_latch_) lost_latch_->CountDown();
}

void OfflineSimulationUser::OnPayload(const std::string& endpoint_id,
                                      Payload payload) {
  payload_ = std::move(payload);
  if (payload_latch_) payload_latch_->CountDown();
}

void OfflineSimulationUser::OnPayloadProgress(const std::string& endpoint_id,
                                              const PayloadProgressInfo& info) {
  MutexLock lock(&progress_mutex_);
  progress_info_ = info;
  if (future_ && predicate_ && predicate_(info)) future_->Set(true);
}

bool OfflineSimulationUser::WaitForProgress(
    std::function<bool(const PayloadProgressInfo&)> predicate,
    absl::Duration timeout) {
  Future<bool> future;
  {
    MutexLock lock(&progress_mutex_);
    if (predicate(progress_info_)) return true;
    future_ = &future;
    predicate_ = std::move(predicate);
  }
  auto response = future.Get(timeout);
  {
    MutexLock lock(&progress_mutex_);
    future_ = nullptr;
    predicate_ = nullptr;
  }
  return response.ok() && response.result();
}

Status OfflineSimulationUser::StartAdvertising(const std::string& service_id,
                                               CountDownLatch* latch) {
  initiated_latch_ = latch;
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

Status OfflineSimulationUser::StartDiscovery(const std::string& service_id,
                                             CountDownLatch* found_latch,
                                             CountDownLatch* lost_latch) {
  found_latch_ = found_latch;
  lost_latch_ = lost_latch;
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

void OfflineSimulationUser::InjectEndpoint(
    const std::string& service_id,
    const OutOfBandConnectionMetadata& metadata) {
  ctrl_.InjectEndpoint(&client_, service_id, metadata);
}

Status OfflineSimulationUser::RequestConnection(CountDownLatch* latch) {
  initiated_latch_ = latch;
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

Status OfflineSimulationUser::AcceptConnection(CountDownLatch* latch) {
  accept_latch_ = latch;
  PayloadListener listener = {
      .payload_cb = absl::bind_front(&OfflineSimulationUser::OnPayload, this),
      .payload_progress_cb =
          absl::bind_front(&OfflineSimulationUser::OnPayloadProgress, this),
  };
  return ctrl_.AcceptConnection(&client_, discovered_.endpoint_id,
                                std::move(listener));
}

Status OfflineSimulationUser::RejectConnection(CountDownLatch* latch) {
  reject_latch_ = latch;
  return ctrl_.RejectConnection(&client_, discovered_.endpoint_id);
}

void OfflineSimulationUser::Disconnect() {
  NEARBY_LOGS(INFO) << "Disconnecting from id=" << discovered_.endpoint_id;
  ctrl_.DisconnectFromEndpoint(&client_, discovered_.endpoint_id);
}

}  // namespace connections
}  // namespace nearby
