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

#include "connections/implementation/offline_service_controller.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "connections/discovery_options.h"
#include "connections/listeners.h"
#include "internal/interop/device.h"

namespace nearby {
namespace connections {

OfflineServiceController::~OfflineServiceController() { Stop(); }

void OfflineServiceController::Stop() {
  NEARBY_LOGS(INFO) << "Initiating shutdown of OfflineServiceController.";
  if (stop_.Set(true)) return;
  payload_manager_.DisconnectFromEndpointManager();
  pcp_manager_.DisconnectFromEndpointManager();
  NEARBY_LOGS(INFO) << "OfflineServiceController has shut down.";
}

Status OfflineServiceController::StartAdvertising(
    ClientProxy* client, const std::string& service_id,
    const AdvertisingOptions& advertising_options,
    const ConnectionRequestInfo& info) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested advertising to start.";
  return pcp_manager_.StartAdvertising(client, service_id, advertising_options,
                                       info);
}

void OfflineServiceController::StopAdvertising(ClientProxy* client) {
  if (stop_) return;
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested advertising to stop.";
  pcp_manager_.StopAdvertising(client);
}

Status OfflineServiceController::StartDiscovery(
    ClientProxy* client, const std::string& service_id,
    const DiscoveryOptions& discovery_options, DiscoveryListener listener) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested discovery to start.";
  return pcp_manager_.StartDiscovery(client, service_id, discovery_options,
                                     std::move(listener));
}

void OfflineServiceController::StopDiscovery(ClientProxy* client) {
  if (stop_) return;
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested discovery to stop.";
  pcp_manager_.StopDiscovery(client);
}

std::pair<Status, std::vector<ConnectionInfoVariant>>
OfflineServiceController::StartListeningForIncomingConnections(
    ClientProxy* client, absl::string_view service_id,
    v3::ConnectionListener listener,
    const v3::ConnectionListeningOptions& options) {
  return pcp_manager_.StartListeningForIncomingConnections(
      client, service_id, std::move(listener), options);
}

void OfflineServiceController::StopListeningForIncomingConnections(
    ClientProxy* client) {
  pcp_manager_.StopListeningForIncomingConnections(client);
}

void OfflineServiceController::InjectEndpoint(
    ClientProxy* client, const std::string& service_id,
    const OutOfBandConnectionMetadata& metadata) {
  if (stop_) return;
  pcp_manager_.InjectEndpoint(client, service_id, metadata);
}

Status OfflineServiceController::RequestConnection(
    ClientProxy* client, const std::string& endpoint_id,
    const ConnectionRequestInfo& info,
    const ConnectionOptions& connection_options) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested a connection to endpoint_id=" << endpoint_id;
  return pcp_manager_.RequestConnection(client, endpoint_id, info,
                                        connection_options);
}

Status OfflineServiceController::RequestConnectionV3(
    ClientProxy* client, const NearbyDevice& remote_device,
    const ConnectionRequestInfo& info,
    const ConnectionOptions& connection_options) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested a connection to endpoint_id="
                    << remote_device.GetEndpointId();
  return pcp_manager_.RequestConnectionV3(client, remote_device, info,
                                          connection_options);
}

Status OfflineServiceController::AcceptConnection(
    ClientProxy* client, const std::string& endpoint_id,
    PayloadListener listener) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " accepted the connection with endpoint_id="
                    << endpoint_id;
  return pcp_manager_.AcceptConnection(client, endpoint_id,
                                       std::move(listener));
}

Status OfflineServiceController::RejectConnection(
    ClientProxy* client, const std::string& endpoint_id) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " rejected the connection with endpoint_id="
                    << endpoint_id;
  return pcp_manager_.RejectConnection(client, endpoint_id);
}

void OfflineServiceController::InitiateBandwidthUpgrade(
    ClientProxy* client, const std::string& endpoint_id) {
  if (stop_) return;
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " initiated a manual bandwidth upgrade with endpoint_id="
                    << endpoint_id;
  bwu_manager_.InitiateBwuForEndpoint(client, endpoint_id);
}

void OfflineServiceController::SendPayload(
    ClientProxy* client, const std::vector<std::string>& endpoint_ids,
    Payload payload) {
  if (stop_) return;
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " is sending payload_id=" << payload.GetId()
                    << " to endpoint_ids={" << absl::StrJoin(endpoint_ids, ",")
                    << "}";
  payload_manager_.SendPayload(client, endpoint_ids, std::move(payload));
}

Status OfflineServiceController::CancelPayload(ClientProxy* client,
                                               std::int64_t payload_id) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " cancelled payload_id=" << payload_id;
  return payload_manager_.CancelPayload(client, payload_id);
}

void OfflineServiceController::DisconnectFromEndpoint(
    ClientProxy* client, const std::string& endpoint_id) {
  if (stop_) return;
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested a disconnection from endpoint_id="
                    << endpoint_id;
  endpoint_manager_.UnregisterEndpoint(client, endpoint_id);
}

Status OfflineServiceController::UpdateAdvertisingOptions(
    ClientProxy* client, absl::string_view service_id,
    const AdvertisingOptions& advertising_options) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  return pcp_manager_.UpdateAdvertisingOptions(client, service_id,
                                               advertising_options);
}

Status OfflineServiceController::UpdateDiscoveryOptions(
    ClientProxy* client, absl::string_view service_id,
    const DiscoveryOptions& discovery_options) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  return pcp_manager_.UpdateDiscoveryOptions(client, service_id,
                                             discovery_options);
}

void OfflineServiceController::SetCustomSavePath(ClientProxy* client,
                                                 const std::string& path) {
  if (stop_) return;
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested to set custom save path: " << path;
  payload_manager_.SetCustomSavePath(client, path);
}

void OfflineServiceController::ShutdownBwuManagerExecutors() {
  bwu_manager_.ShutdownExecutors();
}

}  // namespace connections
}  // namespace nearby
