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

#include "core_v2/internal/mediums/wifi_lan.h"

#include <memory>
#include <string>
#include <utility>

#include "platform_v2/public/logging.h"
#include "platform_v2/public/mutex_lock.h"

namespace location {
namespace nearby {
namespace connections {

bool WifiLan::IsAvailable() const {
  MutexLock lock(&mutex_);

  return IsAvailableLocked();
}

bool WifiLan::IsAvailableLocked() const { return medium_.IsValid(); }

bool WifiLan::StartAdvertising(const std::string& service_id,
                               const std::string& service_info_name) {
  MutexLock lock(&mutex_);

  if (service_info_name.empty()) {
    NEARBY_LOG(
        INFO,
        "Refusing to turn on WifiLan advertising. Empty service info name.");
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOG(INFO,
               "Can't turn on WifiLan advertising. WifiLan is not available.");
    return false;
  }

  if (!medium_.StartAdvertising(service_id, service_info_name)) {
    NEARBY_LOG(
        INFO, "Failed to turn on WifiLan advertising with service info name=%s",
        service_info_name.c_str());
    return false;
  }

  NEARBY_LOGS(INFO) << "Turned on WifiLan advertising with service info name="
                    << service_info_name << ", service id=" << service_id;
  advertising_info_.Add(service_id);
  return true;
}

bool WifiLan::StopAdvertising(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (!IsAdvertisingLocked(service_id)) {
    NEARBY_LOG(INFO, "Can't turn off WifiLan advertising; it is already off");
    return false;
  }

  NEARBY_LOG(INFO, "Turned off WifiLan advertising with service id=%s",
             service_id.c_str());
  bool ret = medium_.StopAdvertising(service_id);
  // Reset our bundle of advertising state to mark that we're no longer
  // advertising.
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
    NEARBY_LOG(INFO,
               "Refusing to start WifiLan discovering with empty service id.");
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOG(
        INFO,
        "Can't discover WifiLan services because WifiLan isn't available.");
    return false;
  }

  if (IsDiscoveringLocked(service_id)) {
    NEARBY_LOG(
        INFO,
        "Refusing to start discovery of WifiLan services because another "
        "discovery is already in-progress.");
    return false;
  }

  if (!medium_.StartDiscovery(service_id, callback)) {
    NEARBY_LOG(INFO, "Failed to start discovery of WifiLan services.");
    return false;
  }

  NEARBY_LOG(INFO, "Turned on WifiLan discovering with service id=%s",
             service_id.c_str());
  // Mark the fact that we're currently performing a WifiLan discovering.
  discovering_info_.Add(service_id);
  return true;
}

bool WifiLan::StopDiscovery(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (!IsDiscoveringLocked(service_id)) {
    NEARBY_LOG(INFO,
               "Can't turn off WifiLan discovering because we never started "
               "discovering.");
    return false;
  }

  NEARBY_LOG(INFO, "Turned off WifiLan discovering with service id=%s",
             service_id.c_str());
  bool ret = medium_.StopDiscovery(service_id);
  discovering_info_.Clear();
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
    NEARBY_LOG(INFO,
               "Refusing to start accepting WifiLan connections with empty "
               "service id.");
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOG(INFO,
               "Can't start accepting WifiLan connections for %s because "
               "WifiLan isn't available.",
               service_id.c_str());
    return false;
  }

  if (IsAcceptingConnectionsLocked(service_id)) {
    NEARBY_LOG(INFO,
               "Refusing to start accepting WifiLan connections for %s because "
               "another WifiLan service socket is already in-progress.",
               service_id.c_str());
    return false;
  }

  if (!medium_.StartAcceptingConnections(service_id, callback)) {
    NEARBY_LOG(INFO, "Failed to accept connections callback for %s.",
               service_id.c_str());
    return false;
  }

  accepting_connections_info_.Add(service_id);
  return true;
}

bool WifiLan::StopAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (!IsAcceptingConnectionsLocked(service_id)) {
    NEARBY_LOG(INFO,
               "Can't stop accepting WifiLan connections because it was never "
               "started.");
    return false;
  }

  bool ret = medium_.StopAcceptingConnections(service_id);
  // Reset our bundle of accepting connections state to mark that we're no
  // longer accepting connections.
  accepting_connections_info_.Remove(service_id);
  return ret;
}

bool WifiLan::IsAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);

  return IsAcceptingConnectionsLocked(service_id);
}

bool WifiLan::IsAcceptingConnectionsLocked(const std::string& service_id) {
  return accepting_connections_info_.Existed(service_id);
}

WifiLanSocket WifiLan::Connect(WifiLanService& wifi_lan_service,
                               const std::string& service_id) {
  MutexLock lock(&mutex_);
  NEARBY_LOG(INFO, "WifiLan::Connect: service=%p, service_info_name=%s",
             &wifi_lan_service, wifi_lan_service.GetName().c_str());
  // Socket to return. To allow for NRVO to work, it has to be a single object.
  WifiLanSocket socket;

  if (service_id.empty()) {
    NEARBY_LOG(INFO,
               "Refusing to create WifiLan socket with empty service_id.");
    return socket;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOG(INFO,
               "Can't create client WifiLan socket [service_id=%s]; WifiLan "
               "isn't available.",
               service_id.c_str());
    return socket;
  }

  socket = medium_.Connect(wifi_lan_service, service_id);
  if (!socket.IsValid()) {
    NEARBY_LOG(INFO, "Failed to Connect via WifiLan [service_id=%s]",
               service_id.c_str());
  }

  return socket;
}

WifiLanService WifiLan::GetRemoteWifiLanService(const std::string& ip_address,
                                                int port) {
  MutexLock lock(&mutex_);
  return medium_.FindRemoteService(ip_address, port);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
