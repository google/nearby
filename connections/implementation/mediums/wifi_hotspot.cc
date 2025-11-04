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

#include <cstdint>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/expected.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/wifi_credential.h"
#include "internal/platform/wifi_hotspot.h"

namespace nearby {
namespace connections {

namespace {
using ::absl::Milliseconds;
using ::location::nearby::proto::connections::OperationResultCode;
}  // namespace

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
    LOG(INFO) << "No need to start Hotspot because it is already started.";
    return true;
  }
  is_hotspot_started_ = medium_.StartWifiHotspot();
  return is_hotspot_started_;
}

bool WifiHotspot::StopWifiHotspot() {
  MutexLock lock(&mutex_);
  if (!is_hotspot_started_) {
    LOG(INFO) << "No need to stop Hotspot because it is not started.";
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

bool WifiHotspot::ConnectWifiHotspot(
    const HotspotCredentials& hotspot_credentials) {
  MutexLock lock(&mutex_);
  if (is_connected_to_hotspot_) {
    LOG(INFO)
        << "No need to connect to Hotspot because it is already connected.";
    return true;
  }
  is_connected_to_hotspot_ = medium_.ConnectWifiHotspot(hotspot_credentials);
  return is_connected_to_hotspot_;
}

bool WifiHotspot::DisconnectWifiHotspot() {
  MutexLock lock(&mutex_);
  if (!is_connected_to_hotspot_) {
    LOG(INFO)
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
    LOG(INFO) << "No server socket found for service_id:" << service_id
              << ".  Use default credentials";
    return crendential;
  }
  it->second.PopulateHotspotCredentials(*crendential);
  return crendential;
}

bool WifiHotspot::StartAcceptingConnections(
    const std::string& service_id, AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    LOG(INFO) << "Can not to start accepting WifiHotspot connections; "
                 "service_id is empty.";
    return false;
  }

  if (!IsAPAvailableLocked()) {
    LOG(INFO) << "Can't start accepting WifiHotspot connections [service_id="
              << service_id << "]; WifiHotspot not available.";
    return false;
  }

  if (IsAcceptingConnectionsLocked(service_id)) {
    LOG(INFO)
        << "Refusing to start accepting WifiHotspot connections [service="
        << service_id
        << "]; WifiHotspot server is already in-progress with the same name.";
    return false;
  }

  // "port=0" to let the platform to select an available port for the socket
  WifiHotspotServerSocket server_socket = medium_.ListenForService();
  if (!server_socket.IsValid()) {
    LOG(INFO)
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
    LOG(INFO) << "Unable to stop accepting WifiHotspot connections because "
                 "the service_id is empty.";
    return false;
  }

  const auto& it = server_sockets_.find(service_id);
  if (it == server_sockets_.end()) {
    LOG(INFO) << "Can't stop accepting WifiHotspot connections for "
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
    LOG(INFO) << "Failed to close WifiHotspot server socket for service_id:"
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

ErrorOr<WifiHotspotSocket> WifiHotspot::Connect(
    const std::string& service_id,
    const HotspotCredentials& hotspot_credentials,
    CancellationFlag* cancellation_flag) {
  MutexLock lock(&mutex_);
  if (service_id.empty()) {
    LOG(INFO) << "Refusing to create client WifiHotspot socket because "
                 "service_id is empty.";
    return {Error(OperationResultCode::NEARBY_LOCAL_CLIENT_STATE_WRONG)};
  }

  if (!IsClientAvailableLocked()) {
    LOG(INFO) << "Can't create client WifiHotspot socket [service_id="
              << service_id << "]; WifiHotspot isn't available.";
    return {Error(
        OperationResultCode::MEDIUM_UNAVAILABLE_WIFI_HOTSPOT_NOT_AVAILABLE)};
  }

  // Try connecting to the service up to wifi_hotspot_max_connection_retries,
  // because it may fail first time if DHCP procedure is not finished yet.
  int64_t wifi_hotspot_max_connection_retries =
      NearbyFlags::GetInstance().GetInt64Flag(
          platform::config_package_nearby::nearby_platform_feature::
              kWifiHotspotConnectionMaxRetries);
  absl::Duration wifi_hotspot_retry_interval =
      Milliseconds(NearbyFlags::GetInstance().GetInt64Flag(
          platform::config_package_nearby::nearby_platform_feature::
              kWifiHotspotConnectionIntervalMillis));
  VLOG(1) << "maximum connection retries="
          << wifi_hotspot_max_connection_retries
          << ", connection interval=" << wifi_hotspot_retry_interval;
  // Socket to return. To allow for NRVO to work, it has to be a single object.
  WifiHotspotSocket socket;
  for (int i = 0; i < wifi_hotspot_max_connection_retries; ++i) {
    for (const ServiceAddress& service_address :
         hotspot_credentials.GetServiceAddresses()) {
      if (cancellation_flag->Cancelled()) {
        LOG(INFO) << "connect to service has been cancelled.";
        return {Error(
            OperationResultCode::
                CLIENT_CANCELLATION_CANCEL_WIFI_HOTSPOT_OUTGOING_CONNECTION)};
      }
      socket =
          medium_.ConnectToService(std::string(service_address.address.begin(),
                                               service_address.address.end()),
                                   service_address.port, cancellation_flag);
      if (socket.IsValid()) {
        break;
      }
    }
    LOG(WARNING) << "reconnect to service at " << (i + 1) << "th times";
    absl::SleepFor(wifi_hotspot_retry_interval);
  }

  if (!socket.IsValid()) {
    LOG(INFO) << "Failed to Connect via WifiHotspot [service_id=" << service_id
              << "]";
    return {
        Error(OperationResultCode::
                  CONNECTIVITY_WIFI_HOTSPOT_CLIENT_SOCKET_CREATION_FAILURE)};
  }

  return socket;
}

}  // namespace connections
}  // namespace nearby
