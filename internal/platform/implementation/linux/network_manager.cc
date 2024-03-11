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

#include <sdbus-c++/Types.h>

#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/network_manager.h"
#include "internal/platform/implementation/linux/network_manager_active_connection.h"

namespace nearby {
namespace linux {
namespace networkmanager {
std::unique_ptr<ActiveConnection>
ObjectManager::GetActiveConnectionForAccessPoint(
    const sdbus::ObjectPath &access_point,
    const sdbus::ObjectPath &device_path) {
  std::map<sdbus::ObjectPath,
           std::map<std::string, std::map<std::string, sdbus::Variant>>>
      objects;
  try {
    objects = GetManagedObjects();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(this, "GetManagedObjects", e);
    return nullptr;
  }

  for (auto &[object_path, interfaces] : objects) {
    if (object_path.find("/org/freedesktop/NetworkManager/ActiveConnection/") ==
        0) {
      if (interfaces.count(org::freedesktop::NetworkManager::Connection::
                               Active_proxy::INTERFACE_NAME) == 1) {
        auto props = interfaces[org::freedesktop::NetworkManager::Connection::
                                    Active_proxy::INTERFACE_NAME];
        sdbus::ObjectPath specific_object = props["SpecificObject"];
        if (specific_object == access_point) {
          std::vector<sdbus::ObjectPath> devices = props["Devices"];
          for (auto &path : devices) {
            if (path == device_path) {
              return std::make_unique<ActiveConnection>(
                  system_bus_, object_path);
            }
          }
        }
      }
    }
  }
  return nullptr;
}

std::unique_ptr<IP4Config> ObjectManager::GetIp4Config(
    const sdbus::ObjectPath &active_connection) {
  std::map<sdbus::ObjectPath,
           std::map<std::string, std::map<std::string, sdbus::Variant>>>
      objects;
  try {
    objects = GetManagedObjects();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(this, "GetManagedObjects", e);
    return nullptr;
  }

  for (auto &[object_path, interfaces] : objects) {
    if (object_path.find("/org/freedesktop/NetworkManager/ActiveConnection/",
                         0) == 0) {
      if (interfaces.count(org::freedesktop::NetworkManager::Connection::
                               Active_proxy::INTERFACE_NAME) == 1) {
        auto props = interfaces[org::freedesktop::NetworkManager::Connection::
                                    Active_proxy::INTERFACE_NAME];
        sdbus::ObjectPath specific_object = props["SpecificObject"];
        if (specific_object == active_connection) {
          sdbus::ObjectPath ip4config = props["Ip4Config"];
          return std::make_unique<IP4Config>(system_bus_, ip4config);
        }
      }
    }
  }

  return nullptr;
}
}  // namespace networkmanager
}  // namespace linux
}  // namespace nearby
