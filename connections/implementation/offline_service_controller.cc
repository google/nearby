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

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/client_proxy.h"
#include "connections/listeners.h"
#include "connections/out_of_band_connection_metadata.h"
#include "connections/params.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "connections/v3/connection_listening_options.h"
#include "connections/v3/listeners.h"
#include "internal/interop/device.h"
#include "internal/platform/logging.h"

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
                    << " requested to start advertising for service_id "
                    << service_id;
  return pcp_manager_.StartAdvertising(client, service_id, advertising_options,
                                       info);
}

void OfflineServiceController::StopAdvertising(ClientProxy* client) {
  if (stop_) return;
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested to stop advertising for service_id "
                    << client->GetAdvertisingServiceId();
  pcp_manager_.StopAdvertising(client);
}

Status OfflineServiceController::StartDiscovery(
    ClientProxy* client, const std::string& service_id,
    const DiscoveryOptions& discovery_options, DiscoveryListener listener) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested to start discovery for service_id "
                    << service_id;
  return pcp_manager_.StartDiscovery(client, service_id, discovery_options,
                                     std::move(listener));
}

void OfflineServiceController::StopDiscovery(ClientProxy* client) {
  if (stop_) return;
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested to stop discovery for service_id "
                    << client->GetDiscoveryServiceId();
  pcp_manager_.StopDiscovery(client);
}

std::pair<Status, std::vector<ConnectionInfoVariant>>
OfflineServiceController::StartListeningForIncomingConnections(
    ClientProxy* client, absl::string_view service_id,
    v3::ConnectionListener listener,
    const v3::ConnectionListeningOptions& options) {
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested to start listening for service_id "
                    << service_id;
  return pcp_manager_.StartListeningForIncomingConnections(
      client, service_id, std::move(listener), options);
}

void OfflineServiceController::StopListeningForIncomingConnections(
    ClientProxy* client) {
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested to stop listening for service_id "
                    << client->GetListeningForIncomingConnectionsServiceId();
  pcp_manager_.StopListeningForIncomingConnections(client);
}

void OfflineServiceController::InjectEndpoint(
    ClientProxy* client, const std::string& service_id,
    const OutOfBandConnectionMetadata& metadata) {
  if (stop_) return;
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested to inject endpoint {endpoint_id:"
                    << metadata.endpoint_id << ", endpoint_info:"
                    << metadata.endpoint_info.AsStringView()
                    << ",remote_bluetooth_mac_address:"
                    << metadata.remote_bluetooth_mac_address.AsStringView()
                    << "} for service_id " << service_id;
  pcp_manager_.InjectEndpoint(client, service_id, metadata);
}

Status OfflineServiceController::RequestConnection(
    ClientProxy* client, const std::string& endpoint_id,
    const ConnectionRequestInfo& info,
    const ConnectionOptions& connection_options) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested a connection to endpoint_id " << endpoint_id;
  return pcp_manager_.RequestConnection(client, endpoint_id, info,
                                        connection_options);
}

Status OfflineServiceController::RequestConnectionV3(
    ClientProxy* client, const NearbyDevice& remote_device,
    const ConnectionRequestInfo& info,
    const ConnectionOptions& connection_options) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested a connection to endpoint_id "
                    << remote_device.GetEndpointId();
  return pcp_manager_.RequestConnectionV3(client, remote_device, info,
                                          connection_options);
}

Status OfflineServiceController::AcceptConnection(
    ClientProxy* client, const std::string& endpoint_id,
    PayloadListener listener) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " accepted the connection from endpoint_id "
                    << endpoint_id;
  return pcp_manager_.AcceptConnection(client, endpoint_id,
                                       std::move(listener));
}

Status OfflineServiceController::RejectConnection(
    ClientProxy* client, const std::string& endpoint_id) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " rejected the connection from endpoint_id "
                    << endpoint_id;
  return pcp_manager_.RejectConnection(client, endpoint_id);
}

void OfflineServiceController::InitiateBandwidthUpgrade(
    ClientProxy* client, const std::string& endpoint_id) {
  if (stop_) return;
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " initiated a manual bandwidth upgrade with endpoint_id "
                    << endpoint_id;
  bwu_manager_.InitiateBwuForEndpoint(client, endpoint_id);
}

void OfflineServiceController::SendPayload(
    ClientProxy* client, const std::vector<std::string>& endpoint_ids,
    Payload payload) {
  if (stop_) return;
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " is sending payload " << payload.GetId()
                    << " to endpoint_ids {" << absl::StrJoin(endpoint_ids, ",")
                    << "}";
  payload_manager_.SendPayload(client, endpoint_ids, std::move(payload));
}

Status OfflineServiceController::CancelPayload(ClientProxy* client,
                                               std::int64_t payload_id) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " cancelled payload " << payload_id;
  return payload_manager_.CancelPayload(client, payload_id);
}

void OfflineServiceController::DisconnectFromEndpoint(
    ClientProxy* client, const std::string& endpoint_id) {
  if (stop_) return;
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested a disconnection from endpoint_id "
                    << endpoint_id;
  endpoint_manager_.UnregisterEndpoint(client, endpoint_id);
}

Status OfflineServiceController::UpdateAdvertisingOptions(
    ClientProxy* client, absl::string_view service_id,
    const AdvertisingOptions& advertising_options) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO)
      << "Client " << client->GetClientId()
      << " requested to update advertising options for service_id "
      << service_id;
  return pcp_manager_.UpdateAdvertisingOptions(client, service_id,
                                               advertising_options);
}

Status OfflineServiceController::UpdateDiscoveryOptions(
    ClientProxy* client, absl::string_view service_id,
    const DiscoveryOptions& discovery_options) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested to update discovery options for service_id "
                    << service_id;
  return pcp_manager_.UpdateDiscoveryOptions(client, service_id,
                                             discovery_options);
}

void OfflineServiceController::SetCustomSavePath(ClientProxy* client,
                                                 const std::string& path) {
  if (stop_) return;
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " requested to set custom save path to " << path;
  payload_manager_.SetCustomSavePath(client, path);
}

void OfflineServiceController::ShutdownBwuManagerExecutors() {
  NEARBY_LOGS(INFO) << "Shutting down BwuManager executors.";
  bwu_manager_.ShutdownExecutors();
}

}  // namespace connections
}  // namespace nearby
