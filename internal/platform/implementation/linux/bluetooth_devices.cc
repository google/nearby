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

#include <functional>
#include <optional>

#include <sdbus-c++/Types.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_devices.h"
#include "internal/platform/implementation/linux/bluez.h"

namespace nearby {
namespace linux {
std::optional<std::reference_wrapper<BluetoothDevice>>
BluetoothDevices::get_device_by_path(
    const sdbus::ObjectPath &device_object_path) {
  absl::ReaderMutexLock l(&devices_by_path_lock_);

  if (devices_by_path_.count(device_object_path) == 0) {
    return std::nullopt;
  }

  auto &device = devices_by_path_.at(device_object_path);
  return *device;
}

std::optional<std::reference_wrapper<BluetoothDevice>>
BluetoothDevices::get_device_by_address(const std::string &addr) {
  auto device_object_path =
      bluez::device_object_path(adapter_object_path_, addr);
  return get_device_by_path(device_object_path);
}

void BluetoothDevices::remove_device_by_path(
    const sdbus::ObjectPath &device_object_path) {
  absl::MutexLock l(&devices_by_path_lock_);

  devices_by_path_.erase(device_object_path);
}

BluetoothDevice &BluetoothDevices::add_new_device(
    sdbus::ObjectPath device_object_path) {
  absl::MutexLock l(&devices_by_path_lock_);
  auto pair = devices_by_path_.emplace(
      std::string(device_object_path),
      std::make_unique<MonitoredBluetoothDevice>(
          system_bus_, std::move(device_object_path), observers_));
  return *pair.first->second;
}
}  // namespace linux
}  // namespace nearby
