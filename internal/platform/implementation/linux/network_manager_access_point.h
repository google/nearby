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

#ifndef PLATFORM_IMPL_LINUX_NETWORK_MANAGER_ACCESS_POINT_H_
#define PLATFORM_IMPL_LINUX_NETWORK_MANAGER_ACCESS_POINT_H_
#include <sdbus-c++/ProxyInterfaces.h>

#include "internal/platform/implementation/linux/generated/dbus/networkmanager/access_point_client.h"

namespace nearby {
namespace linux {
class NetworkManagerAccessPoint
    : public sdbus::ProxyInterfaces<
          org::freedesktop::NetworkManager::AccessPoint_proxy> {
 public:
  NetworkManagerAccessPoint(const NetworkManagerAccessPoint &) = delete;
  NetworkManagerAccessPoint(NetworkManagerAccessPoint &&) = delete;
  NetworkManagerAccessPoint &operator=(const NetworkManagerAccessPoint &) =
      delete;
  NetworkManagerAccessPoint &operator=(NetworkManagerAccessPoint &&) = delete;
  NetworkManagerAccessPoint(sdbus::IConnection &system_bus,
                            sdbus::ObjectPath access_point_object_path)
      : ProxyInterfaces(system_bus, "org.freedesktop.NetworkManager",
                        std::move(access_point_object_path)) {
    registerProxy();
  }
  ~NetworkManagerAccessPoint() { unregisterProxy(); }
};
}  // namespace linux
}  // namespace nearby

#endif
