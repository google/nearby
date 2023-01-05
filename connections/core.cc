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

#include "connections/core.h"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/clock.h"
#include "connections/implementation/service_id_constants.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {

namespace {

// Timeout for ServiceControllerRouter to run StopAllEndpoints.
constexpr absl::Duration kWaitForDisconnect = absl::Milliseconds(10000);

// Verify that |service_id| is not empty and will not conflict with any internal
// service ID formats.
void CheckServiceId(absl::string_view service_id) {
  assert(!service_id.empty());
  assert(service_id != kUnknownServiceId);
  assert(!IsInitiatorUpgradeServiceId(service_id));
}

}  // namespace

Core::Core(ServiceControllerRouter* router) : router_(router) {}

Core::~Core() {
  CountDownLatch latch(1);
  router_->StopAllEndpoints(
      &client_, {
                    .result_cb = [&latch](Status) { latch.CountDown(); },
                });
  if (!latch.Await(kWaitForDisconnect).result()) {
    NEARBY_LOG(FATAL, "Unable to shutdown");
  }
}

Core::Core(Core&&) = default;

Core& Core::operator=(Core&&) = default;

void Core::StartAdvertising(absl::string_view service_id,
                            AdvertisingOptions advertising_options,
                            ConnectionRequestInfo info,
                            ResultCallback callback) {
  CheckServiceId(service_id);
  assert(advertising_options.strategy.IsValid());

  router_->StartAdvertising(&client_, service_id, advertising_options, info,
                            callback);
}

void Core::StopAdvertising(const ResultCallback callback) {
  router_->StopAdvertising(&client_, callback);
}

void Core::StartDiscovery(absl::string_view service_id,
                          DiscoveryOptions discovery_options,
                          DiscoveryListener listener, ResultCallback callback) {
  CheckServiceId(service_id);
  assert(discovery_options.strategy.IsValid());

  router_->StartDiscovery(&client_, service_id, discovery_options, listener,
                          callback);
}

void Core::InjectEndpoint(absl::string_view service_id,
                          OutOfBandConnectionMetadata metadata,
                          ResultCallback callback) {
  CheckServiceId(service_id);
  router_->InjectEndpoint(&client_, service_id, metadata, callback);
}

void Core::StopDiscovery(ResultCallback callback) {
  router_->StopDiscovery(&client_, callback);
}

void Core::RequestConnection(absl::string_view endpoint_id,
                             ConnectionRequestInfo info,
                             ConnectionOptions connection_options,
                             ResultCallback callback) {
  assert(!endpoint_id.empty());

  // Assign the default from feature flags for the keep-alive frame interval and
  // timeout values if client don't mind them or has the unexpected ones.
  if (connection_options.keep_alive_interval_millis == 0 ||
      connection_options.keep_alive_timeout_millis == 0 ||
      connection_options.keep_alive_interval_millis >=
          connection_options.keep_alive_timeout_millis) {
    NEARBY_LOG(
        WARNING,
        "Client request connection with keep-alive frame as interval=%d, "
        "timeout=%d, which is un-expected. Change to default.",
        connection_options.keep_alive_interval_millis,
        connection_options.keep_alive_timeout_millis);
    connection_options.keep_alive_interval_millis =
        FeatureFlags::GetInstance().GetFlags().keep_alive_interval_millis;
    connection_options.keep_alive_timeout_millis =
        FeatureFlags::GetInstance().GetFlags().keep_alive_timeout_millis;
  }

  router_->RequestConnection(&client_, endpoint_id, info, connection_options,
                             callback);
}

void Core::AcceptConnection(absl::string_view endpoint_id,
                            PayloadListener listener, ResultCallback callback) {
  assert(!endpoint_id.empty());

  router_->AcceptConnection(&client_, endpoint_id, listener, callback);
}

void Core::RejectConnection(absl::string_view endpoint_id,
                            ResultCallback callback) {
  assert(!endpoint_id.empty());

  router_->RejectConnection(&client_, endpoint_id, callback);
}

void Core::InitiateBandwidthUpgrade(absl::string_view endpoint_id,
                                    ResultCallback callback) {
  router_->InitiateBandwidthUpgrade(&client_, endpoint_id, callback);
}

void Core::SendPayload(absl::Span<const std::string> endpoint_ids,
                       Payload payload, ResultCallback callback) {
  assert(payload.GetType() != PayloadType::kUnknown);
  assert(!endpoint_ids.empty());

  router_->SendPayload(&client_, endpoint_ids, std::move(payload), callback);
}

void Core::CancelPayload(std::int64_t payload_id, ResultCallback callback) {
  assert(payload_id != 0);

  router_->CancelPayload(&client_, payload_id, callback);
}

void Core::DisconnectFromEndpoint(absl::string_view endpoint_id,
                                  ResultCallback callback) {
  assert(!endpoint_id.empty());

  router_->DisconnectFromEndpoint(&client_, endpoint_id, callback);
}

void Core::StopAllEndpoints(ResultCallback callback) {
  router_->StopAllEndpoints(&client_, callback);
}

void Core::SetCustomSavePath(absl::string_view path, ResultCallback callback) {
  router_->SetCustomSavePath(&client_, path, callback);
}

std::string Core::Dump() { return client_.Dump(); }

}  // namespace connections
}  // namespace nearby
