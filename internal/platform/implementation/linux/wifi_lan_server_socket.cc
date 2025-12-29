// Copyright 2023 Google LLC
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

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <memory>

#include <sdbus-c++/Types.h>

#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/wifi_lan_server_socket.h"
#include "internal/platform/implementation/linux/wifi_lan_socket.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
std::string WifiLanServerSocket::GetIPAddress() const {
  std::vector<sdbus::ObjectPath> connection_paths;
  try {
    connection_paths = network_manager_->ActiveConnections();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(network_manager_, "ActiveConnections", e);
    return std::string();
  }

  for (auto &path : connection_paths) {
    auto active_connection =        
        std::make_unique<networkmanager::ActiveConnection>(system_bus_, path);
    std::string conn_type;
    try {
      conn_type = active_connection->Type();
    } catch (const sdbus::Error &e) {
      DBUS_LOG_PROPERTY_GET_ERROR(active_connection, "Type", e);
      continue;
    }
    if (conn_type == "802-11-wireless" || conn_type == "802-3-ethernet") {
      auto ip4config_path = active_connection->Ip4Config();
      networkmanager::IP4Config ip4config(system_bus_, ip4config_path);
      std::vector<std::map<std::string, sdbus::Variant>> address_data;

      try {
        address_data = ip4config.AddressData();
      } catch (const sdbus::Error &e) {
        DBUS_LOG_PROPERTY_GET_ERROR(&ip4config, "IP4Config", e);
        continue;
      }

      if (address_data.size() > 0) {
        return address_data[0]["address"];
      }
    }
  }

  LOG(ERROR)
      << __func__ << ": Could not find any active IP addresses for this device";
  return std::string();
}

int WifiLanServerSocket::GetPort() const {
 return server_socket_.GetPort();
}

std::unique_ptr<api::WifiLanSocket> WifiLanServerSocket::Accept() {
  auto sock = server_socket_.Accept();
  if (!sock.has_value()) return nullptr;

  return std::make_unique<WifiLanSocket>(std::move(*sock));
}

Exception WifiLanServerSocket::Close() {
  return server_socket_.Close();
}
}  // namespace linux
}  // namespace nearby
