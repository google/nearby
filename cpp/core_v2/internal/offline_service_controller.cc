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

#include "core_v2/internal/offline_service_controller.h"

#include <string>

namespace location {
namespace nearby {
namespace connections {

OfflineServiceController::~OfflineServiceController() { Stop(); }

void OfflineServiceController::Stop() {
  if (stop_.Set(true)) return;
  payload_manager_.DisconnectFromEndpointManager();
  pcp_manager_.DisconnectFromEndpointManager();
}

Status OfflineServiceController::StartAdvertising(
    ClientProxy* client, const std::string& service_id,
    const ConnectionOptions& options, const ConnectionRequestInfo& info) {
  return pcp_manager_.StartAdvertising(client, service_id, options, info);
}

void OfflineServiceController::StopAdvertising(ClientProxy* client) {
  pcp_manager_.StopAdvertising(client);
}

Status OfflineServiceController::StartDiscovery(
    ClientProxy* client, const std::string& service_id,
    const ConnectionOptions& options, const DiscoveryListener& listener) {
  return pcp_manager_.StartDiscovery(client, service_id, options, listener);
}

void OfflineServiceController::StopDiscovery(ClientProxy* client) {
  pcp_manager_.StopDiscovery(client);
}

Status OfflineServiceController::RequestConnection(
    ClientProxy* client, const std::string& endpoint_id,
    const ConnectionRequestInfo& info, const ConnectionOptions& options) {
  return pcp_manager_.RequestConnection(client, endpoint_id, info, options);
}

Status OfflineServiceController::AcceptConnection(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadListener& listener) {
  return pcp_manager_.AcceptConnection(client, endpoint_id, listener);
}

Status OfflineServiceController::RejectConnection(
    ClientProxy* client, const std::string& endpoint_id) {
  return pcp_manager_.RejectConnection(client, endpoint_id);
}

void OfflineServiceController::InitiateBandwidthUpgrade(
    ClientProxy* client, const std::string& endpoint_id) {
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " initiated a manual bandwidth upgrade with endpoint id="
                    << endpoint_id;
  bwu_manager_.InitiateBwuForEndpoint(client, endpoint_id);
}

void OfflineServiceController::SendPayload(
    ClientProxy* client, const std::vector<std::string>& endpoint_ids,
    Payload payload) {
  payload_manager_.SendPayload(client, endpoint_ids, std::move(payload));
}

Status OfflineServiceController::CancelPayload(ClientProxy* client,
                                               std::int64_t payload_id) {
  return payload_manager_.CancelPayload(client, payload_id);
}

void OfflineServiceController::DisconnectFromEndpoint(
    ClientProxy* client, const std::string& endpoint_id) {
  endpoint_manager_.UnregisterEndpoint(client, endpoint_id);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
