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

#include "connections/implementation/mediums/wifi_direct.h"

#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#include "absl/strings/string_view.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/expected.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/wifi_credential.h"
#include "internal/platform/wifi_direct.h"

namespace nearby {
namespace connections {
namespace {
using ::location::nearby::proto::connections::OperationResultCode;
}  // namespace

WifiDirect::WifiDirect() : is_go_started_(false), is_connected_to_go_(false) {
  supported_wifi_direct_auth_types_ = medium_.GetSupportedWifiDirectAuthTypes();
  if (!supported_wifi_direct_auth_types_.empty()) {
    preferred_wifi_direct_auth_type_ =
        supported_wifi_direct_auth_types_.front();
  }
}
WifiDirect::~WifiDirect() {
  while (!server_sockets_.empty()) {
    StopAcceptingConnections(server_sockets_.begin()->first);
  }
  if (is_go_started_) StopWifiDirect();
  if (is_connected_to_go_) DisconnectWifiDirect();

  // All the AcceptLoopRunnable objects in here should already have gotten an
  // opportunity to shut themselves down cleanly in the calls to
  // StopAcceptingConnections() above.
  accept_loops_runner_.Shutdown();
}

bool WifiDirect::IsGOAvailable() const {
  MutexLock lock(&mutex_);

  return IsGOAvailableLocked();
}

bool WifiDirect::IsGOAvailableLocked() const {
  if (medium_.IsValid()) return medium_.IsInterfaceValid();
  return false;
}

bool WifiDirect::IsGCAvailable() const {
  MutexLock lock(&mutex_);

  return IsGCAvailableLocked();
}

bool WifiDirect::IsGCAvailableLocked() const {
  if (medium_.IsValid()) return medium_.IsInterfaceValid();
  return false;
}

bool WifiDirect::IsGOStarted() {
  MutexLock lock(&mutex_);

  return is_go_started_;
}

// Call the Medium to start a softAP and send beacon for Client to scan and
// connect
bool WifiDirect::StartWifiDirect() {
  MutexLock lock(&mutex_);
  if (is_go_started_) {
    LOG(INFO) << "No need to start GO because it is already started.";
    return true;
  }
  is_go_started_ = medium_.StartWifiDirect();
  return is_go_started_;
}

bool WifiDirect::StopWifiDirect() {
  MutexLock lock(&mutex_);
  if (!is_go_started_) {
    LOG(INFO) << "No need to stop GO because it is not started.";
    return true;
  }
  is_go_started_ = false;
  medium_.StopWifiDirect();
  return true;
}

bool WifiDirect::IsConnectedToGO() {
  MutexLock lock(&mutex_);

  return is_connected_to_go_;
}

bool WifiDirect::ConnectWifiDirect(
    const WifiDirectCredentials& wifi_direct_credentials) {
  MutexLock lock(&mutex_);
  if (is_connected_to_go_) {
    LOG(INFO) << "No need to connect to GO because it is already connected.";
    return true;
  }
  is_connected_to_go_ = medium_.ConnectWifiDirect(wifi_direct_credentials);
  return is_connected_to_go_;
}

bool WifiDirect::DisconnectWifiDirect() {
  MutexLock lock(&mutex_);
  if (!is_connected_to_go_) {
    LOG(INFO) << "No need to disconnect to GO because it is not connected.";
    return true;
  }
  is_connected_to_go_ = false;
  return medium_.DisconnectWifiDirect();
}

WifiDirectCredentials* WifiDirect::GetCredentials(
    absl::string_view service_id) {
  MutexLock lock(&mutex_);
  WifiDirectCredentials* crendential = medium_.GetCredential();
  CHECK(crendential);

  const auto& it = server_sockets_.find(service_id);
  if (it == server_sockets_.end()) {
    LOG(INFO) << "No server socket found for service_id:" << service_id
              << ".  Use default credentials";
    return crendential;
  }
  it->second.PopulateWifiDirectCredentials(*crendential);
  return crendential;
}

bool WifiDirect::StartAcceptingConnections(
    const std::string& service_id, AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    LOG(INFO) << "Can not to start accepting WifiDirect GC's connections; "
                 "service_id is empty.";
    return false;
  }

  if (!IsGOAvailableLocked()) {
    LOG(INFO)
        << "Can't start accepting  WifiDirect GC's connections [service_id="
        << service_id << "]; WifiDirct GO is not available.";
    return false;
  }

  if (IsAcceptingConnectionsLocked(service_id)) {
    LOG(INFO)
        << "Refusing to start accepting WifiDirect GC's connections [service="
        << service_id
        << "]; WifiDirect GO server is already in-progress with the same name.";
    return false;
  }

  // "port=0" to let the platform to select an available port for the socket
  WifiDirectServerSocket server_socket = medium_.ListenForService();
  if (!server_socket.IsValid()) {
    LOG(INFO)
        << "Failed to start to listen on WifiDirect GO server for service_id="
        << service_id;
    return false;
  }

  // Mark the fact that there's an in-progress WifiDirect server accepting
  // connections.
  auto owned_server_socket =
      server_sockets_.insert({service_id, std::move(server_socket)})
          .first->second;

  // Start the accept loop on a dedicated thread - this stays alive and
  // listening for new incoming connections until StopAcceptingConnections() is
  // invoked.
  accept_loops_runner_.Execute(
      "wifi-direct-accept",
      [callback = std::move(callback),
       server_socket = std::move(owned_server_socket), service_id]() mutable {
        while (true) {
          WifiDirectSocket client_socket = server_socket.Accept();
          if (!client_socket.IsValid()) {
            server_socket.Close();
            break;
          }
          if (callback) {
            callback(service_id, std::move(client_socket));
          }
        }
      });

  return true;
}

bool WifiDirect::StopAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    LOG(INFO) << "Unable to stop accepting WifiDirect GC's connections because "
                 "the service_id is empty.";
    return false;
  }

  const auto& it = server_sockets_.find(service_id);
  if (it == server_sockets_.end()) {
    LOG(INFO) << "Can't stop accepting WifiDirect GC's connections for "
              << service_id << " because it was never started.";
    return false;
  }

  // Closing the WifiDirectServerSocket will kick off the suicide of the thread
  // in accept_loops_thread_pool_ that block on WifiDirectServerSocket.accept()
  // That may take some time to complete, but there's no particular reason to
  // wait around for it.
  auto item = server_sockets_.extract(it);

  // Store a handle to the WifiDirectServerSocket, so we can use it after
  // removing the entry from server_sockets_; making it scoped
  // is a bonus that takes care of deallocation before we leave this method.
  WifiDirectServerSocket& listening_socket = item.mapped();

  // Regardless of whether or not we fail to close the existing
  // WifiDirectServerSocket, remove it from server_sockets_ so that it
  // frees up this service for another round.

  // Finally, close the WifiDirectServerSocket.
  if (!listening_socket.Close().Ok()) {
    LOG(INFO) << "Failed to close WifiDirect server socket for service_id:"
              << service_id;
    return false;
  }

  return true;
}

bool WifiDirect::IsAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsAcceptingConnectionsLocked(service_id);
}

bool WifiDirect::IsAcceptingConnectionsLocked(const std::string& service_id) {
  return server_sockets_.find(service_id) != server_sockets_.end();
}

ErrorOr<WifiDirectSocket> WifiDirect::Connect(
    const std::string& service_id, const std::string& ip_address, int port,
    CancellationFlag* cancellation_flag) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    LOG(INFO) << "Refusing to create client WifiDirect socket because "
                 "service_id is empty.";
    return {Error(OperationResultCode::NEARBY_LOCAL_CLIENT_STATE_WRONG)};
  }

  if (!IsGCAvailableLocked()) {
    LOG(INFO) << "Can't create WifiDirect client socket [service_id="
              << service_id << "]; WifiDirect GC isn't available.";
    return {Error(
        OperationResultCode::MEDIUM_UNAVAILABLE_WIFI_DIRECT_NOT_AVAILABLE)};
  }

  if (cancellation_flag->Cancelled()) {
    LOG(INFO) << "Can't create  WifiDirect client socket due to cancel";
    return {
        Error(OperationResultCode::
                  CLIENT_CANCELLATION_CANCEL_WIFI_DIRECT_OUTGOING_CONNECTION)};
  }

  // Socket to return. To allow for NRVO to work, it has to be a single object.
  WifiDirectSocket socket;
  socket = medium_.ConnectToService(ip_address, port, cancellation_flag);
  if (!socket.IsValid()) {
    LOG(INFO) << "Failed to Connect via WifiDirect Server [service_id="
              << service_id << "]";
    return {Error(OperationResultCode::
                      CONNECTIVITY_WIFI_DIRECT_CLIENT_SOCKET_CREATION_FAILURE)};
  }

  return socket;
}

bool WifiDirect::SetPreferredWifiDirectAuthType(WifiDirectAuthType auth_type) {
  if (std::find(supported_wifi_direct_auth_types_.begin(),
                supported_wifi_direct_auth_types_.end(),
                auth_type) == supported_wifi_direct_auth_types_.end()) {
    return false;
  }
  preferred_wifi_direct_auth_type_ = auth_type;
  return true;
}

}  // namespace connections
}  // namespace nearby
