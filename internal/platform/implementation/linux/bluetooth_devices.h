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

#include <memory>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>

#include "absl/synchronization/mutex.h"
#include "internal/base/observer_list.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"

namespace nearby {
namespace linux {
class BluetoothDevices final {
 public:
  BluetoothDevices(
      sdbus::IConnection &system_bus, sdbus::ObjectPath adapter_object_path,
      ObserverList<api::BluetoothClassicMedium::Observer> &observers)
      : system_bus_(system_bus),
        observers_(observers),
        adapter_object_path_(std::move(adapter_object_path)) {}

  std::optional<std::reference_wrapper<BluetoothDevice>> get_device_by_path(
      const sdbus::ObjectPath &);
  std::optional<std::reference_wrapper<BluetoothDevice>> get_device_by_address(
      const std::string &);
  void remove_device_by_path(const sdbus::ObjectPath &);
  BluetoothDevice &add_new_device(sdbus::ObjectPath);

 private:
  absl::Mutex devices_by_path_lock_;
  std::map<std::string, std::unique_ptr<MonitoredBluetoothDevice>>
      devices_by_path_;

  sdbus::IConnection &system_bus_;
  ObserverList<api::BluetoothClassicMedium::Observer> &observers_;
  sdbus::ObjectPath adapter_object_path_;
};
}  // namespace linux
}  // namespace nearby

#endif
