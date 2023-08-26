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
#include "connections/implementation/client_proxy.h"
#include "internal/platform/borrowable.h"

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
    ::nearby::Borrowable<ClientProxy*> client, const std::string& service_id,
    const AdvertisingOptions& advertising_options,
    const ConnectionRequestInfo& info) {
  if (stop_) return {Status::kOutOfOrderApiCall};

  ::nearby::Borrowed<ClientProxy*> borrowed = client.Borrow();
  if (!borrowed) {
    NEARBY_LOGS(ERROR) << __func__ << ": ClientProxy is gone";
    return {Status::kError};
  }

  NEARBY_LOGS(INFO) << "Client " << (*borrowed)->GetClientId()
                    << " requested advertising to start.";

  // Mark |borrowed| as `FinishBorrowing()` to prevent Mutex deadlock in
  // `PcpManager::StartAdvertising()`.
  borrowed.FinishBorrowing();
  return pcp_manager_.StartAdvertising(client, service_id, advertising_options,
                                       info);
}

void OfflineServiceController::StopAdvertising(
    ::nearby::Borrowable<ClientProxy*> client) {
  if (stop_) return;

  ::nearby::Borrowed<ClientProxy*> borrowed = client.Borrow();
  if (!borrowed) {
    NEARBY_LOGS(ERROR) << __func__ << ": ClientProxy is gone";
    return;
  }

  NEARBY_LOGS(INFO) << "Client " << (*borrowed)->GetClientId()
                    << " requested advertising to stop.";

  // Mark |borrowed| as `FinishBorrowing()` to prevent Mutex deadlock in
  // `PcpManager::StopAdvertising()`.
  borrowed.FinishBorrowing();
  pcp_manager_.StopAdvertising(client);
}

Status OfflineServiceController::StartDiscovery(
    ::nearby::Borrowable<ClientProxy*> client, const std::string& service_id,
    const DiscoveryOptions& discovery_options,
    const DiscoveryListener& listener) {
  if (stop_) return {Status::kOutOfOrderApiCall};

  ::nearby::Borrowed<ClientProxy*> borrowed = client.Borrow();
  if (!borrowed) {
    NEARBY_LOGS(ERROR) << __func__ << ": ClientProxy is gone";
    return {Status::kError};
  }

  NEARBY_LOGS(INFO) << "Client " << (*borrowed)->GetClientId()
                    << " requested discovery to start.";

  // Mark |borrowed| as `FinishBorrowing()` to prevent Mutex deadlock in
  // `PcpManager::StartDiscovery()`.
  borrowed.FinishBorrowing();
  return pcp_manager_.StartDiscovery(client, service_id, discovery_options,
                                     listener);
}

void OfflineServiceController::StopDiscovery(
    ::nearby::Borrowable<ClientProxy*> client) {
  if (stop_) return;

  ::nearby::Borrowed<ClientProxy*> borrowed = client.Borrow();
  if (!borrowed) {
    NEARBY_LOGS(ERROR) << __func__ << ": ClientProxy is gone";
    return;
  }

  NEARBY_LOGS(INFO) << "Client " << (*borrowed)->GetClientId()
                    << " requested discovery to stop.";

  // Mark |borrowed| as `FinishBorrowing()` to prevent Mutex deadlock in
  // `PcpManager::StopDiscovery()`.
  borrowed.FinishBorrowing();
  pcp_manager_.StopDiscovery(client);
}

std::pair<Status, std::vector<ConnectionInfoVariant>>
OfflineServiceController::StartListeningForIncomingConnections(
    ::nearby::Borrowable<ClientProxy*> client, absl::string_view service_id,
    v3::ConnectionListener listener,
    const v3::ConnectionListeningOptions& options) {
  return pcp_manager_.StartListeningForIncomingConnections(
      client, service_id, std::move(listener), options);
}

void OfflineServiceController::StopListeningForIncomingConnections(
    ::nearby::Borrowable<ClientProxy*> client) {
  pcp_manager_.StopListeningForIncomingConnections(client);
}

void OfflineServiceController::InjectEndpoint(
    ::nearby::Borrowable<ClientProxy*> client, const std::string& service_id,
    const OutOfBandConnectionMetadata& metadata) {
  if (stop_) return;
  pcp_manager_.InjectEndpoint(client, service_id, metadata);
}

Status OfflineServiceController::RequestConnection(
    ::nearby::Borrowable<ClientProxy*> client, const std::string& endpoint_id,
    const ConnectionRequestInfo& info,
    const ConnectionOptions& connection_options) {
  if (stop_) return {Status::kOutOfOrderApiCall};

  ::nearby::Borrowed<ClientProxy*> borrowed = client.Borrow();
  if (!borrowed) {
    NEARBY_LOGS(ERROR) << __func__ << ": ClientProxy is gone";
    return {Status::kError};
  }
  NEARBY_LOGS(INFO) << "Client " << (*borrowed)->GetClientId()
                    << " requested a connection to endpoint_id=" << endpoint_id;

  // Mark |borrowed| as `FinishBorrowing()` to prevent Mutex deadlock in
  // `PcpManager::RequestConnection()`.
  borrowed.FinishBorrowing();
  return pcp_manager_.RequestConnection(client, endpoint_id, info,
                                        connection_options);
}

Status OfflineServiceController::AcceptConnection(
    ::nearby::Borrowable<ClientProxy*> client, const std::string& endpoint_id,
    PayloadListener listener) {
  if (stop_) return {Status::kOutOfOrderApiCall};

  ::nearby::Borrowed<ClientProxy*> borrowed = client.Borrow();
  if (!borrowed) {
    NEARBY_LOGS(ERROR) << __func__ << ": ClientProxy is gone";
    return {Status::kError};
  }

  NEARBY_LOGS(INFO) << "Client " << (*borrowed)->GetClientId()
                    << " accepted the connection with endpoint_id="
                    << endpoint_id;

  // Mark |borrowed| as `FinishBorrowing()` to prevent Mutex deadlock in
  // `PcpManager::AcceptConnection()`.
  borrowed.FinishBorrowing();
  return pcp_manager_.AcceptConnection(client, endpoint_id,
                                       std::move(listener));
}

Status OfflineServiceController::RejectConnection(
    ::nearby::Borrowable<ClientProxy*> client, const std::string& endpoint_id) {
  if (stop_) return {Status::kOutOfOrderApiCall};

  ::nearby::Borrowed<ClientProxy*> borrowed = client.Borrow();
  if (!borrowed) {
    NEARBY_LOGS(ERROR) << __func__ << ": ClientProxy is gone";
    return {Status::kError};
  }

  NEARBY_LOGS(INFO) << "Client " << (*borrowed)->GetClientId()
                    << " rejected the connection with endpoint_id="
                    << endpoint_id;

  // Mark |borrowed| as `FinishBorrowing()` to prevent Mutex deadlock in
  // `PcpManager::RejectConnection()`.
  borrowed.FinishBorrowing();
  return pcp_manager_.RejectConnection(client, endpoint_id);
}

void OfflineServiceController::InitiateBandwidthUpgrade(
    ::nearby::Borrowable<ClientProxy*> client, const std::string& endpoint_id) {
  if (stop_) return;

  ::nearby::Borrowed<ClientProxy*> borrowed = client.Borrow();
  if (!borrowed) {
    NEARBY_LOGS(ERROR) << __func__ << ": ClientProxy is gone";
    return;
  }

  NEARBY_LOGS(INFO) << "Client " << (*borrowed)->GetClientId()
                    << " initiated a manual bandwidth upgrade with endpoint_id="
                    << endpoint_id;

  // Mark |borrowed| as `FinishBorrowing()` to prevent Mutex deadlock in
  // `PcpManager::InitiateBwuForEndpoint()`.
  borrowed.FinishBorrowing();
  bwu_manager_.InitiateBwuForEndpoint(client, endpoint_id);
}

void OfflineServiceController::SendPayload(
    ::nearby::Borrowable<ClientProxy*> client,
    const std::vector<std::string>& endpoint_ids, Payload payload) {
  if (stop_) return;

  ::nearby::Borrowed<ClientProxy*> borrowed = client.Borrow();
  if (!borrowed) {
    NEARBY_LOGS(ERROR) << __func__ << ": ClientProxy is gone";
    return;
  }

  NEARBY_LOGS(INFO) << "Client " << (*borrowed)->GetClientId()
                    << " is sending payload_id=" << payload.GetId()
                    << " to endpoint_ids={" << absl::StrJoin(endpoint_ids, ",")
                    << "}";

  // Mark |borrowed| as `FinishBorrowing()` to prevent Mutex deadlock in
  // `PcpManager::SendPayload()`.
  borrowed.FinishBorrowing();
  payload_manager_.SendPayload(client, endpoint_ids, std::move(payload));
}

Status OfflineServiceController::CancelPayload(
    ::nearby::Borrowable<ClientProxy*> client, std::int64_t payload_id) {
  if (stop_) return {Status::kOutOfOrderApiCall};

  ::nearby::Borrowed<ClientProxy*> borrowed = client.Borrow();
  if (!borrowed) {
    NEARBY_LOGS(ERROR) << __func__ << ": ClientProxy is gone";
    return {Status::kError};
  }

  NEARBY_LOGS(INFO) << "Client " << (*borrowed)->GetClientId()
                    << " cancelled payload_id=" << payload_id;

  // Mark |borrowed| as `FinishBorrowing()` to prevent Mutex deadlock in
  // `PcpManager::CancelPayload()`.
  borrowed.FinishBorrowing();
  return payload_manager_.CancelPayload(client, payload_id);
}

void OfflineServiceController::DisconnectFromEndpoint(
    ::nearby::Borrowable<ClientProxy*> client, const std::string& endpoint_id) {
  if (stop_) return;

  ::nearby::Borrowed<ClientProxy*> borrowed = client.Borrow();
  if (!borrowed) {
    NEARBY_LOGS(ERROR) << __func__ << ": ClientProxy is gone";
    return;
  }

  NEARBY_LOGS(INFO) << "Client " << (*borrowed)->GetClientId()
                    << " requested a disconnection from endpoint_id="
                    << endpoint_id;

  // Mark |borrowed| as `FinishBorrowing()` to prevent Mutex deadlock in
  // `PcpManager::UnregisterEndpoint()`.
  borrowed.FinishBorrowing();
  endpoint_manager_.UnregisterEndpoint(client, endpoint_id);
}

Status OfflineServiceController::UpdateAdvertisingOptions(
    ::nearby::Borrowable<ClientProxy*> client, absl::string_view service_id,
    const AdvertisingOptions& advertising_options) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  return pcp_manager_.UpdateAdvertisingOptions(client, service_id,
                                               advertising_options);
}

Status OfflineServiceController::UpdateDiscoveryOptions(
    ::nearby::Borrowable<ClientProxy*> client, absl::string_view service_id,
    const DiscoveryOptions& discovery_options) {
  if (stop_) return {Status::kOutOfOrderApiCall};
  return pcp_manager_.UpdateDiscoveryOptions(client, service_id,
                                             discovery_options);
}

void OfflineServiceController::SetCustomSavePath(
    ::nearby::Borrowable<ClientProxy*> client, const std::string& path) {
  if (stop_) return;

  ::nearby::Borrowed<ClientProxy*> borrowed = client.Borrow();
  if (!borrowed) {
    NEARBY_LOGS(ERROR) << __func__ << ": ClientProxy is gone";
    return;
  }

  NEARBY_LOGS(INFO) << "Client " << (*borrowed)->GetClientId()
                    << " requested to set custom save path: " << path;

  // Mark |borrowed| as `FinishBorrowing()` to prevent Mutex deadlock in
  // `PcpManager::SetCustomSavePath()`.
  borrowed.FinishBorrowing();
  payload_manager_.SetCustomSavePath(client, path);
}

void OfflineServiceController::ShutdownBwuManagerExecutors() {
  bwu_manager_.ShutdownExecutors();
}

}  // namespace connections
}  // namespace nearby
