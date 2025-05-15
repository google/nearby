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

#include "connections/implementation/mediums/awdl.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "connections/implementation/mediums/multiplex/multiplex_socket.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/medium_selector.h"
#include "internal/platform/awdl.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/expected.h"
#include "internal/platform/implementation/psk_info.h"
#include "internal/platform/implementation/wifi_utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/socket.h"
#include "internal/platform/types.h"

namespace nearby {
namespace connections {

namespace {
using MultiplexSocket = mediums::multiplex::MultiplexSocket;
using location::nearby::proto::connections::OperationResultCode;
}  // namespace

Awdl::~Awdl() {
  // Destructor is not taking locks, but methods it is calling are.
  while (!discovering_info_.service_ids.empty()) {
    StopDiscovery(*discovering_info_.service_ids.begin());
  }
  while (!server_sockets_.empty()) {
    StopAcceptingConnections(server_sockets_.begin()->first);
  }
  while (!advertising_info_.nsd_service_infos.empty()) {
    StopAdvertising(advertising_info_.nsd_service_infos.begin()->first);
  }

  // All the AcceptLoopRunnable objects in here should already have gotten an
  // opportunity to shut themselves down cleanly in the calls to
  // StopAcceptingConnections() above.
  accept_loops_runner_.Shutdown();
}

bool Awdl::IsAvailable() const {
  MutexLock lock(&mutex_);

  return IsAvailableLocked();
}

bool Awdl::IsAvailableLocked() const { return medium_.IsValid(); }

ErrorOr<bool> Awdl::StartAdvertising(const std::string& service_id,
                                     NsdServiceInfo& nsd_service_info) {
  MutexLock lock(&mutex_);

  if (!IsAvailableLocked()) {
    LOG(INFO) << "Can't turn on Awdl advertising. Awdl is not available.";
    return {Error(OperationResultCode::MEDIUM_UNAVAILABLE_LAN_NOT_AVAILABLE)};
  }

  if (!nsd_service_info.IsValid()) {
    LOG(INFO)
        << "Refusing to turn on Awdl advertising. nsd_service_info is not "
           "valid.";
    return {Error(OperationResultCode::MEDIUM_UNAVAILABLE_NSD_NOT_AVAILABLE)};
  }

  if (IsAdvertisingLocked(service_id)) {
    LOG(INFO) << "Failed to Awdl advertise because we're already advertising.";
    return {Error(OperationResultCode::CLIENT_WIFI_LAN_DUPLICATE_ADVERTISING)};
  }

  if (!IsAcceptingConnectionsLocked(service_id)) {
    LOG(INFO) << "Failed to turn on Awdl advertising with nsd_service_info="
              << &nsd_service_info
              << ", service_name=" << nsd_service_info.GetServiceName()
              << ", service_id=" << service_id
              << ". Should accept connections before advertising.";
    return {Error(OperationResultCode::
                      CLIENT_DUPLICATE_ACCEPTING_LAN_CONNECTION_REQUEST)};
  }

  nsd_service_info.SetServiceType(GenerateServiceType(service_id));
  const auto& it = server_sockets_.find(service_id);
  if (it != server_sockets_.end()) {
    nsd_service_info.SetIPAddress(it->second.GetIPAddress());
    nsd_service_info.SetPort(it->second.GetPort());
  }
  if (!medium_.StartAdvertising(nsd_service_info)) {
    LOG(INFO) << "Failed to turn on Awdl advertising with nsd_service_info="
              << &nsd_service_info
              << ", service_name=" << nsd_service_info.GetServiceName()
              << ", service_id=" << service_id;
    return {Error(
        OperationResultCode::CONNECTIVITY_WIFI_LAN_START_ADVERTISING_FAILURE)};
  }

  LOG(INFO) << "Turned on Awdl advertising with nsd_service_info="
            << &nsd_service_info
            << ", service_name=" << nsd_service_info.GetServiceName()
            << ", service_id=" << service_id;
  advertising_info_.Add(service_id, std::move(nsd_service_info));
  return {true};
}

bool Awdl::StopAdvertising(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (!IsAdvertisingLocked(service_id)) {
    LOG(INFO) << "Can't turn off Awdl advertising; it is already off";
    return false;
  }

  LOG(INFO) << "Turned off Awdl advertising with service_id=" << service_id;
  bool ret =
      medium_.StopAdvertising(*advertising_info_.GetServiceInfo(service_id));
  // Reset our bundle of advertising state to mark that we're no longer
  // advertising for specific service_id.
  advertising_info_.Remove(service_id);
  return ret;
}

bool Awdl::IsAdvertising(const std::string& service_id) {
  MutexLock lock(&mutex_);

  return IsAdvertisingLocked(service_id);
}

bool Awdl::IsAdvertisingLocked(const std::string& service_id) {
  return advertising_info_.Existed(service_id);
}

ErrorOr<bool> Awdl::StartDiscovery(const std::string& service_id,
                                   DiscoveredServiceCallback callback) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    LOG(INFO) << "Refusing to start Awdl discovering with empty service_id.";
    return {Error(OperationResultCode::NEARBY_LOCAL_CLIENT_STATE_WRONG)};
  }

  if (!IsAvailableLocked()) {
    LOG(INFO) << "Can't discover Awdl services because Awdl isn't available.";
    return {Error(
        OperationResultCode::MEDIUM_UNAVAILABLE_WIFI_AWARE_NOT_AVAILABLE)};
  }

  if (IsDiscoveringLocked(service_id)) {
    LOG(INFO) << "Refusing to start discovery of Awdl services because another "
                 "discovery is already in-progress.";
    return {Error(OperationResultCode::CLIENT_WIFI_LAN_DUPLICATE_DISCOVERING)};
  }

  std::string service_type = GenerateServiceType(service_id);
  bool ret =
      medium_.StartDiscovery(service_id, service_type, std::move(callback));
  if (!ret) {
    LOG(INFO) << "Failed to start discovery of Awdl services.";
    return {Error(
        OperationResultCode::CONNECTIVITY_WIFI_LAN_START_DISCOVERY_FAILURE)};
  }

  LOG(INFO) << "Turned on Awdl discovering with service_id=" << service_id;
  // Mark the fact that we're currently performing a Awdl discovering.
  discovering_info_.Add(service_id);
  return {true};
}

bool Awdl::StopDiscovery(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (!IsDiscoveringLocked(service_id)) {
    LOG(INFO) << "Can't turn off Awdl discovering because we never started "
                 "discovering.";
    return false;
  }

  std::string service_type = GenerateServiceType(service_id);
  LOG(INFO) << "Turned off Awdl discovering with service_id=" << service_id
            << ", service_type=" << service_type;
  bool ret = medium_.StopDiscovery(service_type);
  discovering_info_.Remove(service_id);
  return ret;
}

bool Awdl::IsDiscovering(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsDiscoveringLocked(service_id);
}

bool Awdl::IsDiscoveringLocked(const std::string& service_id) {
  return discovering_info_.Existed(service_id);
}

ErrorOr<bool> Awdl::StartAcceptingConnections(
    const std::string& service_id, AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);
  return InternalStartAcceptingConnections(service_id, std::nullopt,
                                           std::move(callback));
}

ErrorOr<bool> Awdl::StartAcceptingConnections(
    const std::string& service_id, const api::PskInfo& psk_info,
    AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);
  ErrorOr<bool> result = InternalStartAcceptingConnections(service_id, psk_info,
                                                           std::move(callback));

  if (result.has_value() && result.value()) {
    listening_info_.Add(service_id, psk_info);
  }

  return result;
}

bool Awdl::StopAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    LOG(INFO) << "Unable to stop accepting Awdl connections because "
                 "the service_id is empty.";
    return false;
  }

  const auto& it = server_sockets_.find(service_id);
  if (it == server_sockets_.end()) {
    LOG(INFO) << "Can't stop accepting Awdl connections for " << service_id
              << " because it was never started.";
    return false;
  }

  listening_info_.Remove(service_id);

  // Closing the AwdlServerSocket will kick off the suicide of the thread
  // in accept_loops_thread_pool_ that blocks on AwdlServerSocket.accept().
  // That may take some time to complete, but there's no particular reason to
  // wait around for it.
  auto item = server_sockets_.extract(it);

  // Store a handle to the AwdlServerSocket, so we can use it after
  // removing the entry from server_sockets_; making it scoped
  // is a bonus that takes care of deallocation before we leave this method.
  AwdlServerSocket& listening_socket = item.mapped();

  // Regardless of whether or not we fail to close the existing
  // AwdlServerSocket, remove it from server_sockets_ so that it
  // frees up this service for another round.

  // Finally, close the AwdlServerSocket.
  if (!listening_socket.Close().Ok()) {
    LOG(INFO) << "Failed to close Awdl server socket for service_id="
              << service_id;
    return false;
  }

  return true;
}

bool Awdl::IsAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsAcceptingConnectionsLocked(service_id);
}

bool Awdl::IsAcceptingConnectionsLocked(const std::string& service_id) {
  return server_sockets_.find(service_id) != server_sockets_.end();
}

ErrorOr<AwdlSocket> Awdl::Connect(const std::string& service_id,
                                  const NsdServiceInfo& service_info,
                                  CancellationFlag* cancellation_flag) {
  MutexLock lock(&mutex_);
  return InternalConnect(service_id, service_info, std::nullopt,
                         cancellation_flag);
}

ErrorOr<AwdlSocket> Awdl::Connect(const std::string& service_id,
                                  const NsdServiceInfo& service_info,
                                  const api::PskInfo& psk_info,
                                  CancellationFlag* cancellation_flag) {
  MutexLock lock(&mutex_);
  return InternalConnect(service_id, service_info, psk_info, cancellation_flag);
}

Awdl::AwdlCredential Awdl::GetCredentials(const std::string& service_id) {
  MutexLock lock(&mutex_);
  AwdlCredential credential{};
  NsdServiceInfo* service_info = advertising_info_.GetServiceInfo(service_id);
  if (service_info == nullptr || !service_info->IsValid() ||
      service_info->GetServiceName().empty() ||
      service_info->GetServiceType().empty()) {
    return credential;
  }
  credential.service_name = service_info->GetServiceName();
  credential.service_type = service_info->GetServiceType();

  api::PskInfo* psk_info = listening_info_.GetPskInfo(service_id);
  if (psk_info == nullptr) {
    return credential;
  }
  credential.password = psk_info->password;
  return credential;
}

std::string Awdl::GenerateServiceType(const std::string& service_id) {
  std::string service_id_hash_string;

  const ByteArray service_id_hash = Utils::Sha256Hash(
      service_id, NsdServiceInfo::kTypeFromServiceIdHashLength);
  for (auto byte : std::string(service_id_hash)) {
    absl::StrAppend(&service_id_hash_string, absl::StrFormat("%02X", byte));
  }

  return absl::StrFormat(NsdServiceInfo::kNsdTypeFormat,
                         service_id_hash_string);
}

int Awdl::GeneratePort(const std::string& service_id,
                       std::pair<std::int32_t, std::int32_t> port_range) {
  const std::string service_id_hash =
      std::string(Utils::Sha256Hash(service_id, 4));

  std::uint32_t uint_of_service_id_hash =
      service_id_hash[0] << 24 | service_id_hash[1] << 16 |
      service_id_hash[2] << 8 | service_id_hash[3];

  return port_range.first +
         (uint_of_service_id_hash % (port_range.second - port_range.first));
}

ErrorOr<bool> Awdl::InternalStartAcceptingConnections(
    const std::string& service_id, const std::optional<api::PskInfo>& psk_info,
    AcceptedConnectionCallback callback) {
  if (service_id.empty()) {
    LOG(INFO) << "Refusing to start accepting Awdl connections; "
                 "service_id is empty.";
    return {Error(OperationResultCode::NEARBY_LOCAL_CLIENT_STATE_WRONG)};
  }

  if (!IsAvailableLocked()) {
    LOG(INFO) << "Can't start accepting Awdl connections [service_id="
              << service_id << "]; Awdl not available.";
    return {Error(
        OperationResultCode::MEDIUM_UNAVAILABLE_WIFI_AWARE_NOT_AVAILABLE)};
  }

  if (IsAcceptingConnectionsLocked(service_id)) {
    LOG(INFO) << "Refusing to start accepting Awdl connections [service="
              << service_id
              << "]; Awdl server is already in-progress with the same name.";
    return {Error(OperationResultCode::
                      CLIENT_DUPLICATE_ACCEPTING_LAN_CONNECTION_REQUEST)};
  }

  auto port_range = medium_.GetDynamicPortRange();
  // Generate an exact port here on server socket; if platform doesn't provide
  // range of port then assign 0 to let platform decide it.
  int port = 0;
  if (port_range.has_value() &&
      (port_range->first > 0 && port_range->first <= 65535 &&
       port_range->second > 0 && port_range->second <= 65535 &&
       port_range->first <= port_range->second)) {
    port = GeneratePort(service_id, port_range.value());
  }
  AwdlServerSocket server_socket =
      psk_info.has_value() ? medium_.ListenForService(*psk_info, port)
                           : medium_.ListenForService(port);
  if (!server_socket.IsValid()) {
    LOG(INFO) << "Failed to start accepting Awdl connections for service_id="
              << service_id;
    return {Error(OperationResultCode::
                      CLIENT_CANCELLATION_WIFI_LAN_SERVER_SOCKET_CREATION)};
  }

  // Mark the fact that there's an in-progress Awdl server accepting
  // connections.
  auto owned_server_socket =
      server_sockets_.insert({service_id, std::move(server_socket)})
          .first->second;

  // Start the accept loop on a dedicated thread - this stays alive and
  // listening for new incoming connections until StopAcceptingConnections() is
  // invoked.
  accept_loops_runner_.Execute(
      "awdl-accept",
      [callback = std::move(callback),
       server_socket = std::move(owned_server_socket), service_id]() mutable {
        while (true) {
          AwdlSocket client_socket = server_socket.Accept();
          if (!client_socket.IsValid()) {
            server_socket.Close();
            break;
          }
          LOG(INFO) << "Accepted connection for " << service_id;
          if (callback) {
            LOG(INFO) << "Call back triggered for physical socket.";
            callback(service_id, std::move(client_socket));
          }
        }
      });

  return {true};
}

ErrorOr<AwdlSocket> Awdl::InternalConnect(
    const std::string& service_id, const NsdServiceInfo& service_info,
    const std::optional<api::PskInfo>& psk_info,
    CancellationFlag* cancellation_flag) {
  // Socket to return. To allow for NRVO to work, it has to be a single object.
  AwdlSocket socket;

  if (service_id.empty()) {
    LOG(INFO) << "Refusing to create client Awdl socket because "
                 "service_id is empty.";
    return {Error(OperationResultCode::NEARBY_LOCAL_CLIENT_STATE_WRONG)};
  }

  if (!IsAvailableLocked()) {
    LOG(INFO) << "Can't create client Awdl socket [service_id=" << service_id
              << "]; Awdl isn't available.";
    return {Error(OperationResultCode::MEDIUM_UNAVAILABLE_LAN_NOT_AVAILABLE)};
  }

  if (cancellation_flag->Cancelled()) {
    LOG(INFO) << "Can't create client Awdl socket due to cancel.";
    return {Error(OperationResultCode::
                      CLIENT_CANCELLATION_CANCEL_LAN_OUTGOING_CONNECTION)};
  }

  if (service_info.GetServiceName().empty() ||
      service_info.GetServiceType().empty()) {
    LOG(INFO) << "Can't create client Awdl socket due to invalid service "
                 "information.";
    return {
        Error(OperationResultCode::CONNECTIVITY_WIFI_LAN_INVALID_CREDENTIAL)};
  }

  socket =
      psk_info.has_value()
          ? medium_.ConnectToService(service_info, *psk_info, cancellation_flag)
          : medium_.ConnectToService(service_info, cancellation_flag);
  if (!socket.IsValid()) {
    LOG(INFO) << "Failed to Connect via Awdl [service_id=" << service_id << "]";
    return {Error(
        OperationResultCode::CONNECTIVITY_LAN_CLIENT_SOCKET_CREATION_FAILURE)};
  }

  LOG(INFO) << "Successfully connected via Awdl [service_id=" << service_id
            << "]";
  return socket;
}

}  // namespace connections
}  // namespace nearby
