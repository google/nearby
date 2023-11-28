// Copyright 2022 Google LLC
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

#include "connections/implementation/mediums/wifi_hotspot.h"

#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/mediums/utils.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {

WifiHotspot::~WifiHotspot() {
  while (!server_sockets_.empty()) {
    StopAcceptingConnections(server_sockets_.begin()->first);
  }
  if (is_hotspot_started_) StopWifiHotspot();
  if (is_connected_to_hotspot_) DisconnectWifiHotspot();

  // All the AcceptLoopRunnable objects in here should already have gotten an
  // opportunity to shut themselves down cleanly in the calls to
  // StopAcceptingConnections() above.
  accept_loops_runner_.Shutdown();
}

bool WifiHotspot::IsAPAvailable() const {
  MutexLock lock(&mutex_);

  return IsAPAvailableLocked();
}

bool WifiHotspot::IsAPAvailableLocked() const {
  if (medium_.IsValid()) return medium_.IsInterfaceValid();
  return false;
}

bool WifiHotspot::IsClientAvailable() const {
  MutexLock lock(&mutex_);

  return IsClientAvailableLocked();
}

bool WifiHotspot::IsClientAvailableLocked() const { return medium_.IsValid(); }

bool WifiHotspot::IsHotspotStarted() {
  MutexLock lock(&mutex_);

  return is_hotspot_started_;
}

// Call the Medium to start a softAP and send beacon for Client to scan and
// connect
bool WifiHotspot::StartWifiHotspot() {
  MutexLock lock(&mutex_);
  if (is_hotspot_started_) {
    NEARBY_LOGS(INFO)
        << "No need to start Hotspot because it is already started.";
    return true;
  }
  is_hotspot_started_ = medium_.StartWifiHotspot();
  return is_hotspot_started_;
}

bool WifiHotspot::StopWifiHotspot() {
  MutexLock lock(&mutex_);
  if (!is_hotspot_started_) {
    NEARBY_LOGS(INFO) << "No need to stop Hotspot because it is not started.";
    return true;
  }
  is_hotspot_started_ = false;
  medium_.StopWifiHotspot();
  return true;
}

bool WifiHotspot::IsConnectedToHotspot() {
  MutexLock lock(&mutex_);

  return is_connected_to_hotspot_;
}

bool WifiHotspot::ConnectWifiHotspot(const std::string& ssid,
                                     const std::string& password,
                                     int frequency) {
  MutexLock lock(&mutex_);
  if (is_connected_to_hotspot_) {
    NEARBY_LOGS(INFO)
        << "No need to connect to Hotspot because it is already connected.";
    return true;
  }
  is_connected_to_hotspot_ =
      medium_.ConnectWifiHotspot(ssid, password, frequency);
  return is_connected_to_hotspot_;
}

bool WifiHotspot::DisconnectWifiHotspot() {
  MutexLock lock(&mutex_);
  if (!is_connected_to_hotspot_) {
    NEARBY_LOGS(INFO)
        << "No need to disconnect to Hotspot because it is not connected.";
    return true;
  }
  is_connected_to_hotspot_ = false;
  medium_.DisconnectWifiHotspot();
  return true;
}

HotspotCredentials* WifiHotspot::GetCredentials(absl::string_view service_id) {
  MutexLock lock(&mutex_);
  HotspotCredentials* crendential = medium_.GetCredential();
  CHECK(crendential);

  const auto& it = server_sockets_.find(service_id);
  if (it == server_sockets_.end()) {
    NEARBY_LOGS(INFO) << "No server socket found for service_id:" << service_id
                      << ".  Use default credentials";
    return crendential;
  }
  crendential->SetGateway(it->second.GetIPAddress());
  crendential->SetIPAddress(it->second.GetIPAddress());
  crendential->SetPort(it->second.GetPort());

  return crendential;
}

bool WifiHotspot::StartAcceptingConnections(
    const std::string& service_id, AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    NEARBY_LOGS(INFO) << "Can not to start accepting WifiHotspot connections; "
                         "service_id is empty.";
    return false;
  }

  if (!IsAPAvailableLocked()) {
    NEARBY_LOGS(INFO)
        << "Can't start accepting WifiHotspot connections [service_id="
        << service_id << "]; WifiHotspot not available.";
    return false;
  }

  if (IsAcceptingConnectionsLocked(service_id)) {
    NEARBY_LOGS(INFO)
        << "Refusing to start accepting WifiHotspot connections [service="
        << service_id
        << "]; WifiHotspot server is already in-progress with the same name.";
    return false;
  }

  // "port=0" to let the platform to select an available port for the socket
  WifiHotspotServerSocket server_socket = medium_.ListenForService(/*port=*/0);
  if (!server_socket.IsValid()) {
    NEARBY_LOGS(INFO)
        << "Failed to start accepting WifiHotspot connections for service_id="
        << service_id;
    return false;
  }

  // Mark the fact that there's an in-progress WifiHotspot server accepting
  // connections.
  auto owned_server_socket =
      server_sockets_.insert({service_id, std::move(server_socket)})
          .first->second;

  // Start the accept loop on a dedicated thread - this stays alive and
  // listening for new incoming connections until StopAcceptingConnections() is
  // invoked.
  accept_loops_runner_.Execute(
      "wifi-hotspot-accept",
      [callback = std::move(callback),
       server_socket = std::move(owned_server_socket), service_id]() mutable {
        while (true) {
          WifiHotspotSocket client_socket = server_socket.Accept();
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

bool WifiHotspot::StopAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    NEARBY_LOGS(INFO)
        << "Unable to stop accepting WifiHotspot connections because "
           "the service_id is empty.";
    return false;
  }

  const auto& it = server_sockets_.find(service_id);
  if (it == server_sockets_.end()) {
    NEARBY_LOGS(INFO) << "Can't stop accepting WifiHotspot connections for "
                      << service_id << " because it was never started.";
    return false;
  }

  // Closing the WifiHotspotServerSocket will kick off the suicide of the thread
  // in accept_loops_thread_pool_ that block on WifiHotspotServerSocket.accept()
  // That may take some time to complete, but there's no particular reason to
  // wait around for it.
  auto item = server_sockets_.extract(it);

  // Store a handle to the WifiHotspotServerSocket, so we can use it after
  // removing the entry from server_sockets_; making it scoped
  // is a bonus that takes care of deallocation before we leave this method.
  WifiHotspotServerSocket& listening_socket = item.mapped();

  // Regardless of whether or not we fail to close the existing
  // WifiHotspotServerSocket, remove it from server_sockets_ so that it
  // frees up this service for another round.

  // Finally, close the WifiHotspotServerSocket.
  if (!listening_socket.Close().Ok()) {
    NEARBY_LOGS(INFO)
        << "Failed to close WifiHotspot server socket for service_id:"
        << service_id;
    return false;
  }

  return true;
}

bool WifiHotspot::IsAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsAcceptingConnectionsLocked(service_id);
}

bool WifiHotspot::IsAcceptingConnectionsLocked(const std::string& service_id) {
  return server_sockets_.find(service_id) != server_sockets_.end();
}

WifiHotspotSocket WifiHotspot::Connect(const std::string& service_id,
                                       const std::string& ip_address, int port,
                                       CancellationFlag* cancellation_flag) {
  MutexLock lock(&mutex_);
  // Socket to return. To allow for NRVO to work, it has to be a single object.
  WifiHotspotSocket socket;

  if (service_id.empty()) {
    NEARBY_LOGS(INFO) << "Refusing to create client WifiHotspot socket because "
                         "service_id is empty.";
    return socket;
  }

  if (!IsClientAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't create client WifiHotspot socket [service_id="
                      << service_id << "]; WifiHotspot isn't available.";
    return socket;
  }

  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(INFO) << "Can't create client WifiHotspot socket due to cancel";
    return socket;
  }

  socket = medium_.ConnectToService(ip_address, port, cancellation_flag);
  if (!socket.IsValid()) {
    NEARBY_LOGS(INFO) << "Failed to Connect via WifiHotspot [service_id="
                      << service_id << "]";
  }

  return socket;
}

}  // namespace connections
}  // namespace nearby
