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

#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_DEVICES_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_DEVICES_H_

#include <chrono>
#include <memory>

#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "internal/base/observer_list.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"

namespace nearby {
namespace linux {
class BluetoothDevices final {
 public:
  BluetoothDevices(
      std::shared_ptr<sdbus::IConnection> system_bus,
      sdbus::ObjectPath adapter_object_path,
      ObserverList<api::BluetoothClassicMedium::Observer> &observers)
      : system_bus_(std::move(system_bus)),
        observers_(observers),
        adapter_object_path_(std::move(adapter_object_path)) {}

  std::shared_ptr<BluetoothDevice> get_device_by_path(const sdbus::ObjectPath &)
      ABSL_LOCKS_EXCLUDED(devices_by_path_lock_);
  std::shared_ptr<BluetoothDevice> get_device_by_address(const std::string &);
  std::shared_ptr<BluetoothDevice> get_device_by_unique_id(
      api::ble_v2::BlePeripheral::UniqueId id) {
      // TODO: Should probably remove BlePeripheral stuff from here but we can keep it since we can convert to/from
      // uint64_t
      MacAddress tmp;
      MacAddress::FromUint64(id, tmp);
    return get_device_by_address(tmp.ToString());
  }

  std::shared_ptr<MonitoredBluetoothDevice> add_new_device(sdbus::ObjectPath)
      ABSL_LOCKS_EXCLUDED(devices_by_path_lock_);

  void remove_device_by_path(const sdbus::ObjectPath &)
      ABSL_LOCKS_EXCLUDED(devices_by_path_lock_);
  void mark_peripheral_lost(const sdbus::ObjectPath &)
      ABSL_LOCKS_EXCLUDED(devices_by_path_lock_);
  void cleanup_lost_peripherals() ABSL_LOCKS_EXCLUDED(devices_by_path_lock_);

 private:
  std::shared_ptr<sdbus::IConnection> system_bus_;
  ObserverList<api::BluetoothClassicMedium::Observer> &observers_;
  sdbus::ObjectPath adapter_object_path_;

  absl::Mutex devices_by_path_lock_;
  absl::flat_hash_map<std::string, std::shared_ptr<MonitoredBluetoothDevice>>
      devices_by_path_ ABSL_GUARDED_BY(devices_by_path_lock_);
  std::chrono::time_point<std::chrono::steady_clock> last_cleanup_
      ABSL_GUARDED_BY(devices_by_path_lock_);
};

class DeviceWatcher final : sdbus::ProxyInterfaces<sdbus::ObjectManager_proxy> {
 public:
  DeviceWatcher(const DeviceWatcher &) = delete;
  DeviceWatcher(DeviceWatcher &&) = delete;
  DeviceWatcher &operator=(const DeviceWatcher &) = delete;
  DeviceWatcher &operator=(DeviceWatcher &&) = delete;

  DeviceWatcher(
      sdbus::IConnection &system_bus,
      const sdbus::ObjectPath &adapter_object_path,
      std::shared_ptr<BluetoothDevices> devices,
      std::unique_ptr<api::BluetoothClassicMedium::DiscoveryCallback>
          discovery_callback,
      std::shared_ptr<ObserverList<api::BluetoothClassicMedium::Observer>>
          observers)
      : ProxyInterfaces(system_bus, "org.bluez", "/"),
        adapter_object_path_(adapter_object_path),
        devices_(std::move(devices)),
        discovery_cb_(std::move(discovery_callback)),
        observers_(std::move(observers)) {
    notifyExistingDevices();
    registerProxy();
  }
  DeviceWatcher(sdbus::IConnection &system_bus,
                const sdbus::ObjectPath &adapter_object_path,
                std::shared_ptr<BluetoothDevices> devices)
      : DeviceWatcher(system_bus, adapter_object_path, std::move(devices),
                      nullptr, nullptr) {}
  ~DeviceWatcher() { unregisterProxy(); }

  void onInterfacesAdded(
      const sdbus::ObjectPath &object,
      const std::map<std::string, std::map<std::string, sdbus::Variant>>
          &interfaces) override;
  void onInterfacesRemoved(const sdbus::ObjectPath &object,
                           const std::vector<std::string> &interfaces) override;

 private:
  void notifyExistingDevices();

  sdbus::ObjectPath adapter_object_path_;
  std::shared_ptr<BluetoothDevices> devices_;
  std::shared_ptr<api::BluetoothClassicMedium::DiscoveryCallback> discovery_cb_;
  std::shared_ptr<ObserverList<api::BluetoothClassicMedium::Observer>>
      observers_;
};

}  // namespace linux
}  // namespace nearby

#endif
