// Copyright 2025 Google LLC
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

#include "connections/implementation/mediums/wifi_aware.h"

#include <string>
#include <utility>

#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/expected.h"
#include "internal/platform/implementation/upgrade_address_info.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/wifi_aware.h"

namespace nearby {
namespace connections {

WifiAware::~WifiAware() {
  while (!discovering_info_.empty()) {
    StopDiscovery(*discovering_info_.begin());
  }
  while (!server_sockets_.empty()) {
    StopAcceptingConnections(server_sockets_.begin()->first);
  }
  while (!advertising_info_.empty()) {
    StopAdvertising(advertising_info_.begin()->first);
  }
  accept_loops_runner_.Shutdown();
}

bool WifiAware::IsAvailable() const {
  MutexLock lock(&mutex_);
  return IsAvailableLocked();
}

bool WifiAware::IsAvailableLocked() const {
  if (!NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kEnableWifiAware)) {
    return false;
  }
  return medium_.IsValid();
}

ErrorOr<bool> WifiAware::StartAdvertising(
    const std::string& service_id, const NsdServiceInfo& nsd_service_info,
    AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);

  if (!IsAvailableLocked()) {
    LOG(INFO)
        << "Can't turn on WifiAware advertising. WifiAware is not available.";
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_WIFI_AWARE_NOT_AVAILABLE)};
  }

  if (IsAdvertisingLocked(service_id)) {
    LOG(INFO)
        << "Failed to WifiAware advertise because we're already advertising.";
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      NEARBY_WIFI_AWARE_OPERATION_REGISTERED_FAILED)};
  }

  auto accept_result =
      StartAcceptingConnectionsLocked(service_id, std::move(callback));
  if (accept_result.has_error()) {
    return accept_result;
  }

  NsdServiceInfo nsd_service_info_copy(nsd_service_info);
  nsd_service_info_copy.SetPort(0);

  if (!medium_.StartAdvertising(nsd_service_info_copy)) {
    LOG(INFO) << "Failed to turn on WifiAware advertising for service_id="
              << service_id;
    StopAcceptingConnectionsLocked(service_id);
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      CONNECTIVITY_WIFI_AWARE_START_ADVERTISING_FAILURE)};
  }

  LOG(INFO) << "Turned on WifiAware advertising for service_id=" << service_id;
  advertising_info_.emplace(service_id, std::move(nsd_service_info_copy));
  return {true};
}

bool WifiAware::StopAdvertising(const std::string& service_id) {
  MutexLock lock(&mutex_);

  auto it = advertising_info_.find(service_id);
  if (it == advertising_info_.end()) {
    LOG(INFO) << "Can't turn off WifiAware advertising; it is already off";
    return false;
  }

  LOG(INFO) << "Turned off WifiAware advertising for service_id=" << service_id;
  bool ret = medium_.StopAdvertising(it->second);
  advertising_info_.erase(it);
  StopAcceptingConnectionsLocked(service_id);
  return ret;
}

bool WifiAware::IsAdvertising(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsAdvertisingLocked(service_id);
}

bool WifiAware::IsAdvertisingLocked(const std::string& service_id) {
  return advertising_info_.contains(service_id);
}

ErrorOr<bool> WifiAware::StartDiscovery(const std::string& service_id,
                                        DiscoveredServiceCallback callback) {
  MutexLock lock(&mutex_);

  if (!IsAvailableLocked()) {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_WIFI_AWARE_NOT_AVAILABLE)};
  }

  if (IsDiscoveringLocked(service_id)) {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      NEARBY_WIFI_AWARE_OPERATION_REGISTERED_FAILED)};
  }

  if (medium_.StartDiscovery(service_id, std::move(callback))) {
    LOG(INFO) << "Turned on WifiAware discovering for service_id="
              << service_id;
    discovering_info_.insert(service_id);
    return {true};
  } else {
    LOG(INFO) << "Failed to start discovery of WifiAware services.";
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      CONNECTIVITY_WIFI_AWARE_START_DISCOVERY_FAILURE)};
  }
}

bool WifiAware::StopDiscovery(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (discovering_info_.erase(service_id) == 0) {
    return false;
  }

  LOG(INFO) << "Turned off WifiAware discovering for service_id=" << service_id;
  medium_.StopDiscovery(service_id);
  return true;
}

bool WifiAware::IsDiscovering(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsDiscoveringLocked(service_id);
}

bool WifiAware::IsDiscoveringLocked(const std::string& service_id) {
  return discovering_info_.contains(service_id);
}

ErrorOr<bool> WifiAware::StartAcceptingConnections(
    const std::string& service_id, AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);

  if (!IsAvailableLocked()) {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_WIFI_AWARE_NOT_AVAILABLE)};
  }

  if (IsAcceptingConnectionsLocked(service_id)) {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      NEARBY_WIFI_AWARE_OPERATION_REGISTERED_FAILED)};
  }

  return StartAcceptingConnectionsLocked(service_id, std::move(callback));
}

bool WifiAware::StopAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return StopAcceptingConnectionsLocked(service_id);
}

bool WifiAware::IsAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsAcceptingConnectionsLocked(service_id);
}

bool WifiAware::IsAcceptingConnectionsLocked(const std::string& service_id) {
  return server_sockets_.contains(service_id);
}

ErrorOr<bool> WifiAware::StartAcceptingConnectionsLocked(
    const std::string& service_id, AcceptedConnectionCallback callback) {
  if (medium_.StartPublishing()) {
    LOG(INFO) << "WifiAware successfully started publishing";

    WifiAwareServerSocket server_socket = medium_.ListenForService(0);
    if (!server_socket.IsValid()) {
      LOG(INFO)
          << "Failed to start accepting WifiAware connections for service_id="
          << service_id;
      return {
          Error(location::nearby::proto::connections::OperationResultCode::
                    CONNECTIVITY_WIFI_AWARE_SERVER_SOCKET_CREATION_FAILURE)};
    }

    auto owned_server_socket =
        server_sockets_.insert({service_id, std::move(server_socket)})
            .first->second;

    accept_loops_runner_.Execute(
        "wifi-aware-accept",
        [callback = std::move(callback),
         server_socket = std::move(owned_server_socket), service_id]() mutable {
          while (true) {
            WifiAwareSocket client_socket = server_socket.Accept();
            if (!client_socket.IsValid()) {
              server_socket.Close();
              break;
            }
            if (callback) {
              callback(service_id, std::move(client_socket));
            }
          }
        });

    return {true};
  } else {
    LOG(ERROR) << "WifiAware failed to start publishing";
    return {false};
  }
}

bool WifiAware::StopAcceptingConnectionsLocked(const std::string& service_id) {
  auto it = server_sockets_.find(service_id);
  if (it == server_sockets_.end()) {
    return false;
  }

  WifiAwareServerSocket& listening_socket = it->second;
  listening_socket.Close();
  server_sockets_.erase(it);

  if (server_sockets_.empty()) {
    medium_.StopPublishing();
  }
  return true;
}

bool WifiAware::IsPublishing() {
  MutexLock lock(&mutex_);
  return medium_.IsPublishing();
}

bool WifiAware::StartPublishing() {
  MutexLock lock(&mutex_);
  return medium_.StartPublishing();
}

bool WifiAware::StopPublishing() {
  MutexLock lock(&mutex_);
  return medium_.StopPublishing();
}

bool WifiAware::IsSubscribing() {
  MutexLock lock(&mutex_);
  return medium_.IsSubscribing();
}

bool WifiAware::StartSubscribing() {
  MutexLock lock(&mutex_);
  return medium_.StartSubscribing();
}

bool WifiAware::StopSubscribing() {
  MutexLock lock(&mutex_);
  return medium_.StopSubscribing();
}

ErrorOr<WifiAwareSocket> WifiAware::Connect(
    const std::string& service_id, const NsdServiceInfo& service_info,
    CancellationFlag* cancellation_flag) {
  MutexLock lock(&mutex_);
  if (!IsAvailableLocked()) {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_WIFI_AWARE_NOT_AVAILABLE)};
  }
  if (cancellation_flag != nullptr && cancellation_flag->Cancelled()) {
    return {
        Error(location::nearby::proto::connections::OperationResultCode::
                  CLIENT_CANCELLATION_CANCEL_WIFI_AWARE_OUTGOING_CONNECTION)};
  }

  WifiAwareSocket socket =
      medium_.ConnectToService(service_info, cancellation_flag);
  if (!socket.IsValid()) {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      CONNECTIVITY_WIFI_AWARE_CLIENT_SOCKET_CREATION_FAILURE)};
  }
  return {std::move(socket)};
}

api::UpgradeAddressInfo WifiAware::GetUpgradeAddressCandidates(
    const std::string& service_id) {
  MutexLock lock(&mutex_);
  const auto& it = server_sockets_.find(service_id);
  if (it == server_sockets_.end()) {
    return {};
  }
  return medium_.GetUpgradeAddressCandidates(it->second);
}

}  // namespace connections
}  // namespace nearby
