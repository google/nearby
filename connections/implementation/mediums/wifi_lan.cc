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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "connections/implementation/mediums/multiplex/multiplex_socket.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/medium_selector.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/socket.h"
#include "internal/platform/types.h"
#include "internal/platform/wifi_lan.h"

namespace nearby {
namespace connections {

namespace {
using MultiplexSocket = mediums::multiplex::MultiplexSocket;
}  // namespace

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
  {
    MutexLock lock(&mutex_);
    if (is_multiplex_enabled_) {
      NEARBY_LOGS(INFO) << "Closing multiplex sockets for "
                        << multiplex_sockets_.size() << " IPs";
      for (auto& [ip_addr, multiplex_socket] : multiplex_sockets_) {
        NEARBY_LOGS(INFO) << "Closing multiplex sockets for: " << ip_addr;
        multiplex_socket->~MultiplexSocket();
      }
      multiplex_sockets_.clear();
    }
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
  bool ret =
      medium_.StartDiscovery(service_id, service_type, std::move(callback));
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

  // Register the callback to listen for incoming multiplex virtual socket.
  if (is_multiplex_enabled_) {
    MultiplexSocket::ListenForIncomingConnection(
        service_id, Medium::WIFI_LAN,
        [&callback](const std::string& listening_service_id,
                    MediumSocket* virtual_socket) mutable {
          if (callback) {
            callback(listening_service_id,
                     *(down_cast<WifiLanSocket*>(virtual_socket)));
          }
        });
  }
  // Start the accept loop on a dedicated thread - this stays alive and
  // listening for new incoming connections until StopAcceptingConnections() is
  // invoked.
  accept_loops_runner_.Execute(
      "wifi-lan-accept", [callback = std::move(callback),
                          server_socket = std::move(owned_server_socket),
                          service_id, this]() mutable {
        while (true) {
          WifiLanSocket client_socket = server_socket.Accept();
          if (!client_socket.IsValid()) {
            server_socket.Close();
            break;
          }
          NEARBY_LOGS(INFO) << "Accepted connection for " << service_id;
          bool callback_called = false;
          {
            MutexLock lock(&mutex_);
            if (is_multiplex_enabled_) {
              // Observed from the log that when the sender tries to connect to
              // the receiver's server socket, the server side will somehow
              // receive 3 connection request events(don’t know what’s happening
              // in Windows’s lower layer code). The 2nd normally is the real
              // one. The other two will result in a failed data receiving in
              // Windows platform layer. To avoid creating multiplex
              // IncomingSocket, we will check if the first read is successful
              // or not. If not, discard it. If yes, save that packet
              // content(the first frame length), then create the multiplex
              // socket, then feed that content to that multiplex socket.
              ExceptionOr<std::int32_t> read_int =
                  Base64Utils::ReadInt(&client_socket.GetInputStream());
              if (!read_int.ok()) {
                NEARBY_LOGS(WARNING)
                    << __func__
                    << "Failed to read. Exception:" << read_int.exception()
                    << "Discard the connection.";
                continue;
              }
              WifiLanSocket client_socket_bak = client_socket;
              auto physical_socket_ptr =
                  std::make_shared<WifiLanSocket>(client_socket_bak);

              MultiplexSocket* multiplex_socket =
                  MultiplexSocket::CreateIncomingSocket(
                      physical_socket_ptr, service_id, read_int.result());
              if (multiplex_socket != nullptr &&
                  multiplex_socket->GetVirtualSocket(service_id)) {
                multiplex_sockets_.emplace(server_socket.GetIPAddress(),
                                           multiplex_socket);
                MultiplexSocket::StopListeningForIncomingConnection(
                    service_id, Medium::WIFI_LAN);
                NEARBY_LOGS(INFO) << "Multiplex virtaul socket created for "
                                  << server_socket.GetIPAddress();
                if (callback) {
                  callback(
                      service_id,
                      *(down_cast<WifiLanSocket*>(
                          multiplex_socket->GetVirtualSocket(service_id))));
                  callback_called = true;
                }
              }
            }
          }
          if (callback && !callback_called) {
                NEARBY_LOGS(INFO) << "Call back triggered for physical socket.";
            callback(service_id, std::move(client_socket));
          }
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
  if (is_multiplex_enabled_) {
    MultiplexSocket::StopListeningForIncomingConnection(service_id,
                                                        Medium::WIFI_LAN);
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

  ExceptionOr<WifiLanSocket> virtual_socket =
      ConnectWithMultiplexSocketLocked(service_id, service_info.GetIPAddress());
  if (virtual_socket.ok()) {
    return virtual_socket.result();
  }

  socket = medium_.ConnectToService(service_info, cancellation_flag);
  if (!socket.IsValid()) {
    NEARBY_LOGS(INFO) << "Failed to Connect via WifiLan [service_id="
                      << service_id << "]";
    return socket;
  } else {
    ExceptionOr<WifiLanSocket> virtual_socket =
        CreateOutgoingMultiplexSocketLocked(socket, service_id,
                                            service_info.GetIPAddress());
    if (virtual_socket.ok()) {
      NEARBY_LOGS(INFO)
          << "Successfully connected via Multiplex WifiLan [service_id="
          << service_id << "]";
      return virtual_socket.result();
    }
  }

  NEARBY_LOGS(INFO) << "Successfully connected via WifiLan [service_id="
                    << service_id << "]";
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

  ExceptionOr<WifiLanSocket> virtual_socket =
      ConnectWithMultiplexSocketLocked(service_id, ip_address);
  if (virtual_socket.ok()) {
    return virtual_socket.result();
  }

  socket = medium_.ConnectToService(ip_address, port, cancellation_flag);
  if (!socket.IsValid()) {
    NEARBY_LOGS(INFO) << "Failed to Connect via WifiLan [service_id="
                      << service_id << "]";
    return socket;
  } else {
    ExceptionOr<WifiLanSocket> virtual_socket =
        CreateOutgoingMultiplexSocketLocked(socket, service_id, ip_address);
    if (virtual_socket.ok()) {
      NEARBY_LOGS(INFO)
          << "Successfully connected via Multiplex WifiLan [service_id="
          << service_id << "]";
      return virtual_socket.result();
    }
  }

  NEARBY_LOGS(INFO) << "Successfully connected via WifiLan [service_id="
                    << service_id << "]";
  return socket;
}

ExceptionOr<WifiLanSocket> WifiLan::ConnectWithMultiplexSocketLocked(
    const std::string& service_id, const std::string& ip_address) {
  if (is_multiplex_enabled_) {
    NEARBY_LOGS(INFO) << "multiplex_sockets_ size:"
                      << multiplex_sockets_.size();
    auto it = multiplex_sockets_.find(ip_address);
    if (it != multiplex_sockets_.end()) {
      MultiplexSocket* multiplex_socket = it->second;
      if (multiplex_socket->IsShutdown()) {
        NEARBY_LOGS(INFO)
            << "Erase multiplex_socket(already shutdown) for ip_address: "
            << WifiUtils::GetHumanReadableIpAddress(ip_address);
        multiplex_socket->~MultiplexSocket();
        multiplex_sockets_.erase(it);
        return ExceptionOr<WifiLanSocket>(Exception::kFailed);
      }
      if (multiplex_socket->IsEnabled()) {
        auto* virtual_socket =
            multiplex_socket->EstablishVirtualSocket(service_id);
        // Should not happen.
        auto* wlan_socket = down_cast<WifiLanSocket*>(virtual_socket);
        if (wlan_socket == nullptr) {
          NEARBY_LOGS(INFO) << "Failed to cast to WifiLanSocket for "
                            << service_id << " with ip_address: "
                            << WifiUtils::GetHumanReadableIpAddress(ip_address);
          return ExceptionOr<WifiLanSocket>(Exception::kFailed);
        }
        return ExceptionOr<WifiLanSocket>(*wlan_socket);
      }
    }
  }
  return ExceptionOr<WifiLanSocket>(Exception::kFailed);
}

ExceptionOr<WifiLanSocket> WifiLan::CreateOutgoingMultiplexSocketLocked(
    WifiLanSocket& socket, const std::string& service_id,
    const std::string& ip_address) {
  if (is_multiplex_enabled_) {
    // Create MultiplexSocket, but set it to be disabled as default. It will be
    // enabled if both side support multiplex for WIFI_LAN
    auto physical_socket_ptr = std::make_shared<WifiLanSocket>(socket);
    MultiplexSocket* multiplex_socket =
        MultiplexSocket::CreateOutgoingSocket(physical_socket_ptr, service_id);

    auto* virtual_socket = multiplex_socket->GetVirtualSocket(service_id);
    // Should not happen.
    auto* wlan_socket = down_cast<WifiLanSocket*>(virtual_socket);
    if (wlan_socket == nullptr) {
      NEARBY_LOGS(INFO) << "Failed to cast to WifiLanSocket for " << service_id
                        << " with ip_address: "
                        << WifiUtils::GetHumanReadableIpAddress(ip_address);
      return ExceptionOr<WifiLanSocket>(Exception::kFailed);
    }
    NEARBY_LOGS(INFO) << "Multiplex socket created for ip_address: "
                      << WifiUtils::GetHumanReadableIpAddress(ip_address);
    multiplex_sockets_.emplace(ip_address,
                               multiplex_socket);
    return ExceptionOr<WifiLanSocket>(*wlan_socket);
  }
  return ExceptionOr<WifiLanSocket>(Exception::kFailed);
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
