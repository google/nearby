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

#ifndef PLATFORM_IMPL_LINUX_BLUEZ_ADVERTISEMENT_MONITOR_MANAGER_H_
#define PLATFORM_IMPL_LINUX_BLUEZ_ADVERTISEMENT_MONITOR_MANAGER_H_

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/ProxyInterfaces.h>

#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/advertisement_monitor_manager_client.h"

namespace nearby {
namespace linux {
namespace bluez {
class AdvertisementMonitorManager final
    : public sdbus::ProxyInterfaces<
          org::bluez::AdvertisementMonitorManager1_proxy> {
 private:
  friend std::unique_ptr<AdvertisementMonitorManager>
  std::make_unique<AdvertisementMonitorManager>(
      sdbus::IConnection &, const ::nearby::linux::BluetoothAdapter &);
  AdvertisementMonitorManager(
      sdbus::IConnection &system_bus,
      const ::nearby::linux::BluetoothAdapter &adapter)
      : ProxyInterfaces(system_bus, "org.bluez", adapter.GetObjectPath()) {
    registerProxy();
  }

 public:
  AdvertisementMonitorManager(const AdvertisementMonitorManager &) = delete;
  AdvertisementMonitorManager(AdvertisementMonitorManager &&) = delete;
  AdvertisementMonitorManager &operator=(const AdvertisementMonitorManager &) =
      delete;
  AdvertisementMonitorManager &operator=(AdvertisementMonitorManager &&) =
      delete;
  ~AdvertisementMonitorManager() { unregisterProxy(); }

  static std::unique_ptr<AdvertisementMonitorManager>
  DiscoverAdvertisementMonitorManager(
      sdbus::IConnection &system_bus,
      const ::nearby::linux::BluetoothAdapter &adapter) {
    bluez::BluezObjectManager manager(system_bus);
    std::map<sdbus::ObjectPath,
             std::map<std::string, std::map<std::string, sdbus::Variant>>>
        objects;
    try {
      objects = manager.GetManagedObjects();
    } catch (const sdbus::Error &e) {
      DBUS_LOG_METHOD_CALL_ERROR(&manager, "GetManagedObjects", e);
      return nullptr;
    }
    if (objects.count(adapter.GetObjectPath()) == 0) {
      LOG(ERROR) << __func__ << ": Adapter object no longer exists "
                         << adapter.GetObjectPath();
      return nullptr;
    }

    if (objects[adapter.GetObjectPath()].count(
            org::bluez::AdvertisementMonitorManager1_proxy::INTERFACE_NAME) ==
        0) {
      LOG(ERROR)
          << __func__ << ": Adapter " << adapter.GetObjectPath()
          << " doesn't provide "
          << org::bluez::AdvertisementMonitorManager1_proxy::INTERFACE_NAME;
      return nullptr;
    }

    return std::make_unique<AdvertisementMonitorManager>(system_bus, adapter);
  }
};
}  // namespace bluez
}  // namespace linux
}  // namespace nearby

#endif
