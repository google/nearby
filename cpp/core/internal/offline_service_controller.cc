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

#include "core/internal/offline_service_controller.h"

#include <cinttypes>
#include <string>

#include "absl/strings/str_join.h"

namespace location {
namespace nearby {
namespace connections {

OfflineServiceController::~OfflineServiceController() { Stop(); }

void OfflineServiceController::Stop() {
  NEARBY_LOG(INFO, "Initiating shutdown of OfflineServiceController.");
  if (stop_.Set(true)) return;
  payload_manager_.DisconnectFromEndpointManager();
  pcp_manager_.DisconnectFromEndpointManager();
  NEARBY_LOG(INFO, "OfflineServiceController has shut down.");
}

Status OfflineServiceController::StartAdvertising(
    ClientProxy* client, const std::string& service_id,
    const ConnectionOptions& options, const ConnectionRequestInfo& info) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOG(INFO, "Client %" PRIx64 " requested advertising to start.",
             client->GetClientId());
  return pcp_manager_.StartAdvertising(client, service_id, options, info);
}

void OfflineServiceController::StopAdvertising(ClientProxy* client) {
  if (stop_) return;
  NEARBY_LOG(INFO, "Client %" PRIx64 " requested advertising to stop.",
             client->GetClientId());
  pcp_manager_.StopAdvertising(client);
}

Status OfflineServiceController::StartDiscovery(
    ClientProxy* client, const std::string& service_id,
    const ConnectionOptions& options, const DiscoveryListener& listener) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOG(INFO, "Client %" PRIx64 " requested discovery to start.",
             client->GetClientId());
  return pcp_manager_.StartDiscovery(client, service_id, options, listener);
}

void OfflineServiceController::StopDiscovery(ClientProxy* client) {
  if (stop_) return;
  NEARBY_LOG(INFO, "Client %" PRIx64 " requested discovery to stop.",
             client->GetClientId());
  pcp_manager_.StopDiscovery(client);
}

void OfflineServiceController::InjectEndpoint(
    ClientProxy* client, const std::string& service_id,
    const OutOfBandConnectionMetadata& metadata) {
  if (stop_) return;
  pcp_manager_.InjectEndpoint(client, service_id, metadata);
}

Status OfflineServiceController::RequestConnection(
    ClientProxy* client, const std::string& endpoint_id,
    const ConnectionRequestInfo& info, const ConnectionOptions& options) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOG(INFO,
             "Client %" PRIx64 " requested a connection to endpoint_id=%s",
             client->GetClientId(), endpoint_id.c_str());
  return pcp_manager_.RequestConnection(client, endpoint_id, info, options);
}

Status OfflineServiceController::AcceptConnection(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadListener& listener) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOG(INFO,
             "Client %" PRIx64 " accepted the connection with endpoint_id=%s",
             client->GetClientId(), endpoint_id.c_str());
  return pcp_manager_.AcceptConnection(client, endpoint_id, listener);
}

Status OfflineServiceController::RejectConnection(
    ClientProxy* client, const std::string& endpoint_id) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOG(INFO,
             "Client %" PRIx64 " rejected the connection with endpoint_id=%s",
             client->GetClientId(), endpoint_id.c_str());
  return pcp_manager_.RejectConnection(client, endpoint_id);
}

void OfflineServiceController::InitiateBandwidthUpgrade(
    ClientProxy* client, const std::string& endpoint_id) {
  if (stop_) return;
  NEARBY_LOG(INFO,
             "Client %" PRIx64
             " initiated a manual bandwidth upgrade with endpoint_id=%s",
             client->GetClientId(), endpoint_id.c_str());
  bwu_manager_.InitiateBwuForEndpoint(client, endpoint_id);
}

void OfflineServiceController::SendPayload(
    ClientProxy* client, const std::vector<std::string>& endpoint_ids,
    Payload payload) {
  if (stop_) return;
  NEARBY_LOG(INFO,
             "Client %" PRIx64 " is sending payload_id=%" PRIx64
             " to endpoint_ids={%s}",
             client->GetClientId(), static_cast<std::int64_t>(payload.GetId()),
             absl::StrJoin(endpoint_ids, ",").c_str());
  payload_manager_.SendPayload(client, endpoint_ids, std::move(payload));
}

Status OfflineServiceController::CancelPayload(ClientProxy* client,
                                               std::int64_t payload_id) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOG(INFO, "Client %" PRIx64 " cancelled payload_id=%" PRIx64,
             client->GetClientId(), static_cast<int64_t>(payload_id));
  return payload_manager_.CancelPayload(client, payload_id);
}

void OfflineServiceController::DisconnectFromEndpoint(
    ClientProxy* client, const std::string& endpoint_id) {
  if (stop_) return;
  NEARBY_LOG(
      INFO, "Client %" PRIx64 " requested a disconnection from endpoint_id=%s",
      client->GetClientId(), endpoint_id.c_str());
  endpoint_manager_.UnregisterEndpoint(client, endpoint_id);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
