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

#include "connections/implementation/mediums/wifi_lan.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "connections/implementation/mediums/utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace location {
namespace nearby {
namespace connections {

WifiLan::~WifiLan() {
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

bool WifiLan::IsAvailable() const {
  MutexLock lock(&mutex_);

  return IsAvailableLocked();
}

bool WifiLan::IsAvailableLocked() const { return medium_.IsValid(); }

bool WifiLan::StartAdvertising(const std::string& service_id,
                               NsdServiceInfo& nsd_service_info) {
  MutexLock lock(&mutex_);

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO)
        << "Can't turn on WifiLan advertising. WifiLan is not available.";
    return false;
  }

  if (!nsd_service_info.IsValid()) {
    NEARBY_LOGS(INFO)
        << "Refusing to turn on WifiLan advertising. nsd_service_info is not "
           "valid.";
    return false;
  }

  if (IsAdvertisingLocked(service_id)) {
    NEARBY_LOGS(INFO)
        << "Failed to WifiLan advertise because we're already advertising.";
    return false;
  }

  if (!IsAcceptingConnectionsLocked(service_id)) {
    NEARBY_LOGS(INFO)
        << "Failed to turn on WifiLan advertising with nsd_service_info="
        << &nsd_service_info
        << ", service_name=" << nsd_service_info.GetServiceName()
        << ", service_id=" << service_id
        << ". Should accept connections before advertising.";
    return false;
  }

  nsd_service_info.SetServiceType(GenerateServiceType(service_id));
  const auto& it = server_sockets_.find(service_id);
  if (it != server_sockets_.end()) {
    nsd_service_info.SetIPAddress(it->second.GetIPAddress());
    nsd_service_info.SetPort(it->second.GetPort());
  }
  if (!medium_.StartAdvertising(nsd_service_info)) {
    NEARBY_LOGS(INFO)
        << "Failed to turn on WifiLan advertising with nsd_service_info="
        << &nsd_service_info
        << ", service_name=" << nsd_service_info.GetServiceName()
        << ", service_id=" << service_id;
    return false;
  }

  NEARBY_LOGS(INFO) << "Turned on WifiLan advertising with nsd_service_info="
                    << &nsd_service_info
                    << ", service_name=" << nsd_service_info.GetServiceName()
                    << ", service_id=" << service_id;
  advertising_info_.Add(service_id, std::move(nsd_service_info));
  return true;
}

bool WifiLan::StopAdvertising(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (!IsAdvertisingLocked(service_id)) {
    NEARBY_LOGS(INFO)
        << "Can't turn off WifiLan advertising; it is already off";
    return false;
  }

  NEARBY_LOGS(INFO) << "Turned off WifiLan advertising with service_id="
                    << service_id;
  bool ret =
      medium_.StopAdvertising(*advertising_info_.GetServiceInfo(service_id));
  // Reset our bundle of advertising state to mark that we're no longer
  // advertising for specific service_id.
  advertising_info_.Remove(service_id);
  return ret;
}

bool WifiLan::IsAdvertising(const std::string& service_id) {
  MutexLock lock(&mutex_);

  return IsAdvertisingLocked(service_id);
}

bool WifiLan::IsAdvertisingLocked(const std::string& service_id) {
  return advertising_info_.Existed(service_id);
}

bool WifiLan::StartDiscovery(const std::string& service_id,
                             DiscoveredServiceCallback callback) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    NEARBY_LOGS(INFO)
        << "Refusing to start WifiLan discovering with empty service_id.";
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO)
        << "Can't discover WifiLan services because WifiLan isn't available.";
    return false;
  }

  if (IsDiscoveringLocked(service_id)) {
    NEARBY_LOGS(INFO)
        << "Refusing to start discovery of WifiLan services because another "
           "discovery is already in-progress.";
    return false;
  }

  std::string service_type = GenerateServiceType(service_id);
  bool ret = medium_.StartDiscovery(service_id, service_type, callback);
  if (!ret) {
    NEARBY_LOGS(INFO) << "Failed to start discovery of WifiLan services.";
    return false;
  }

  NEARBY_LOGS(INFO) << "Turned on WifiLan discovering with service_id="
                    << service_id;
  // Mark the fact that we're currently performing a WifiLan discovering.
  discovering_info_.Add(service_id);
  return true;
}

bool WifiLan::StopDiscovery(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (!IsDiscoveringLocked(service_id)) {
    NEARBY_LOGS(INFO)
        << "Can't turn off WifiLan discovering because we never started "
           "discovering.";
    return false;
  }

  std::string service_type = GenerateServiceType(service_id);
  NEARBY_LOGS(INFO) << "Turned off WifiLan discovering with service_id="
                    << service_id << ", service_type=" << service_type;
  bool ret = medium_.StopDiscovery(service_type);
  discovering_info_.Remove(service_id);
  return ret;
}

bool WifiLan::IsDiscovering(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsDiscoveringLocked(service_id);
}

bool WifiLan::IsDiscoveringLocked(const std::string& service_id) {
  return discovering_info_.Existed(service_id);
}

bool WifiLan::StartAcceptingConnections(const std::string& service_id,
                                        AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    NEARBY_LOGS(INFO) << "Refusing to start accepting WifiLan connections; "
                         "service_id is empty.";
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO)
        << "Can't start accepting WifiLan connections [service_id="
        << service_id << "]; WifiLan not available.";
    return false;
  }

  if (IsAcceptingConnectionsLocked(service_id)) {
    NEARBY_LOGS(INFO)
        << "Refusing to start accepting WifiLan connections [service="
        << service_id
        << "]; WifiLan server is already in-progress with the same name.";
    return false;
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
  WifiLanServerSocket server_socket = medium_.ListenForService(port);
  if (!server_socket.IsValid()) {
    NEARBY_LOGS(INFO)
        << "Failed to start accepting WifiLan connections for service_id="
        << service_id;
    return false;
  }

  // Mark the fact that there's an in-progress WifiLan server accepting
  // connections.
  auto owned_server_socket =
      server_sockets_.insert({service_id, std::move(server_socket)})
          .first->second;

  // Start the accept loop on a dedicated thread - this stays alive and
  // listening for new incoming connections until StopAcceptingConnections() is
  // invoked.
  accept_loops_runner_.Execute(
      "wifi-lan-accept",
      [callback = std::move(callback),
       server_socket = std::move(owned_server_socket), service_id]() mutable {
        while (true) {
          WifiLanSocket client_socket = server_socket.Accept();
          if (!client_socket.IsValid()) {
            server_socket.Close();
            break;
          }
          callback.accepted_cb(service_id, std::move(client_socket));
        }
      });

  return true;
}

bool WifiLan::StopAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    NEARBY_LOGS(INFO) << "Unable to stop accepting WifiLan connections because "
                         "the service_id is empty.";
    return false;
  }

  const auto& it = server_sockets_.find(service_id);
  if (it == server_sockets_.end()) {
    NEARBY_LOGS(INFO) << "Can't stop accepting WifiLan connections for "
                      << service_id << " because it was never started.";
    return false;
  }

  // Closing the WifiLanServerSocket will kick off the suicide of the thread
  // in accept_loops_thread_pool_ that blocks on WifiLanServerSocket.accept().
  // That may take some time to complete, but there's no particular reason to
  // wait around for it.
  auto item = server_sockets_.extract(it);

  // Store a handle to the WifiLanServerSocket, so we can use it after
  // removing the entry from server_sockets_; making it scoped
  // is a bonus that takes care of deallocation before we leave this method.
  WifiLanServerSocket& listening_socket = item.mapped();

  // Regardless of whether or not we fail to close the existing
  // WifiLanServerSocket, remove it from server_sockets_ so that it
  // frees up this service for another round.

  // Finally, close the WifiLanServerSocket.
  if (!listening_socket.Close().Ok()) {
    NEARBY_LOGS(INFO) << "Failed to close WifiLan server socket for service_id="
                      << service_id;
    return false;
  }

  return true;
}

bool WifiLan::IsAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsAcceptingConnectionsLocked(service_id);
}

bool WifiLan::IsAcceptingConnectionsLocked(const std::string& service_id) {
  return server_sockets_.find(service_id) != server_sockets_.end();
}

WifiLanSocket WifiLan::Connect(const std::string& service_id,
                               const NsdServiceInfo& service_info,
                               CancellationFlag* cancellation_flag) {
  MutexLock lock(&mutex_);
  // Socket to return. To allow for NRVO to work, it has to be a single object.
  WifiLanSocket socket;

  if (service_id.empty()) {
    NEARBY_LOGS(INFO) << "Refusing to create client WifiLan socket because "
                         "service_id is empty.";
    return socket;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't create client WifiLan socket [service_id="
                      << service_id << "]; WifiLan isn't available.";
    return socket;
  }

  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(INFO) << "Can't create client WifiLan socket due to cancel.";
    return socket;
  }

  socket = medium_.ConnectToService(service_info, cancellation_flag);
  if (!socket.IsValid()) {
    NEARBY_LOGS(INFO) << "Failed to Connect via WifiLan [service_id="
                      << service_id << "]";
  }

  return socket;
}

WifiLanSocket WifiLan::Connect(const std::string& service_id,
                               const std::string& ip_address, int port,
                               CancellationFlag* cancellation_flag) {
  MutexLock lock(&mutex_);
  // Socket to return. To allow for NRVO to work, it has to be a single object.
  WifiLanSocket socket;

  if (service_id.empty()) {
    NEARBY_LOGS(INFO) << "Refusing to create client WifiLan socket because "
                         "service_id is empty.";
    return socket;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't create client WifiLan socket [service_id="
                      << service_id << "]; WifiLan isn't available.";
    return socket;
  }

  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(INFO) << "Can't create client WifiLan socket due to cancel.";
    return socket;
  }

  socket = medium_.ConnectToService(ip_address, port, cancellation_flag);
  if (!socket.IsValid()) {
    NEARBY_LOGS(INFO) << "Failed to Connect via WifiLan [service_id="
                      << service_id << "]";
  }

  return socket;
}

std::pair<std::string, int> WifiLan::GetCredentials(
    const std::string& service_id) {
  MutexLock lock(&mutex_);
  const auto& it = server_sockets_.find(service_id);
  if (it == server_sockets_.end()) {
    return std::pair<std::string, int>();
  }
  return std::pair<std::string, int>(it->second.GetIPAddress(),
                                     it->second.GetPort());
}

std::string WifiLan::GenerateServiceType(const std::string& service_id) {
  std::string service_id_hash_string;

  const ByteArray service_id_hash = Utils::Sha256Hash(
      service_id, NsdServiceInfo::kTypeFromServiceIdHashLength);
  for (auto byte : std::string(service_id_hash)) {
    absl::StrAppend(&service_id_hash_string, absl::StrFormat("%02X", byte));
  }

  return absl::StrFormat(NsdServiceInfo::kNsdTypeFormat,
                         service_id_hash_string);
}

int WifiLan::GeneratePort(const std::string& service_id,
                          std::pair<std::int32_t, std::int32_t> port_range) {
  const std::string service_id_hash =
      std::string(Utils::Sha256Hash(service_id, 4));

  std::uint32_t uint_of_service_id_hash =
      service_id_hash[0] << 24 | service_id_hash[1] << 16 |
      service_id_hash[2] << 8 | service_id_hash[3];

  return port_range.first +
         (uint_of_service_id_hash % (port_range.second - port_range.first));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
