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

#include "absl/functional/bind_front.h"
#include "connections/listeners.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/system_clock.h"

namespace nearby {
namespace connections {

void SimulationUser::OnConnectionInitiated(const std::string& endpoint_id,
                                           const ConnectionResponseInfo& info,
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

void SimulationUser::OnConnectionAccepted(const std::string& endpoint_id) {
  if (accept_latch_) accept_latch_->CountDown();
}

void SimulationUser::OnConnectionRejected(const std::string& endpoint_id,
                                          Status status) {
  if (reject_latch_) reject_latch_->CountDown();
}

void SimulationUser::OnEndpointFound(const std::string& endpoint_id,
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

void SimulationUser::OnEndpointLost(const std::string& endpoint_id) {
  if (lost_latch_) lost_latch_->CountDown();
}

void SimulationUser::OnPayload(const std::string& endpoint_id,
                               Payload payload) {
  payload_ = std::move(payload);
  if (payload_latch_) payload_latch_->CountDown();
}

void SimulationUser::OnPayloadProgress(const std::string& endpoint_id,
                                       const PayloadProgressInfo& info) {
  MutexLock lock(&progress_mutex_);
  progress_info_ = info;
  if (future_ && predicate_ && predicate_(info)) future_->Set(true);
}

bool SimulationUser::WaitForProgress(
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

void SimulationUser::StartAdvertising(const std::string& service_id,
                                      CountDownLatch* latch) {
  initiated_latch_ = latch;
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
                                    CountDownLatch* latch) {
  found_latch_ = latch;
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

void SimulationUser::InjectEndpoint(
    const std::string& service_id,
    const OutOfBandConnectionMetadata& metadata) {
  mgr_.InjectEndpoint(&client_, service_id, metadata);
}

void SimulationUser::RequestConnection(CountDownLatch* latch) {
  initiated_latch_ = latch;
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

void SimulationUser::AcceptConnection(CountDownLatch* latch) {
  accept_latch_ = latch;
  PayloadListener listener = {
      .payload_cb = absl::bind_front(&SimulationUser::OnPayload, this),
      .payload_progress_cb =
          absl::bind_front(&SimulationUser::OnPayloadProgress, this),
  };
  EXPECT_TRUE(mgr_.AcceptConnection(&client_, discovered_.endpoint_id,
                                    std::move(listener))
                  .Ok());
}

void SimulationUser::RejectConnection(CountDownLatch* latch) {
  reject_latch_ = latch;
  EXPECT_TRUE(mgr_.RejectConnection(&client_, discovered_.endpoint_id).Ok());
}

}  // namespace connections
}  // namespace nearby
