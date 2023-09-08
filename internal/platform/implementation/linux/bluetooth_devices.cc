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

#include <chrono>
#include <functional>
#include <optional>

#include <absl/time/clock.h>
#include <sdbus-c++/Types.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_devices.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
static constexpr std::chrono::minutes kLostPeripheralsCleanupMinFreq(5);

std::shared_ptr<BluetoothDevice> BluetoothDevices::get_device_by_path(
    const sdbus::ObjectPath &device_object_path) {
  absl::ReaderMutexLock l(&devices_by_path_lock_);

  if (devices_by_path_.count(device_object_path) == 0) {
    return nullptr;
  }

  return devices_by_path_[device_object_path];
}

std::shared_ptr<BluetoothDevice> BluetoothDevices::get_device_by_address(
    const std::string &addr) {
  auto device_object_path =
      bluez::device_object_path(adapter_object_path_, addr);
  return get_device_by_path(device_object_path);
}

void BluetoothDevices::remove_device_by_path(
    const sdbus::ObjectPath &device_object_path) {
  absl::MutexLock l(&devices_by_path_lock_);

  devices_by_path_.erase(device_object_path);
}

void BluetoothDevices::mark_peripheral_lost(
    const sdbus::ObjectPath &device_object_path) {
  absl::ReaderMutexLock lock(&devices_by_path_lock_);
  if (devices_by_path_.count(device_object_path) == 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Device " << device_object_path
                       << " doesn't exist";
  }
  devices_by_path_[device_object_path]->MarkLost();
}

void BluetoothDevices::cleanup_lost_peripherals() {
  auto now = std::chrono::steady_clock::now();
  absl::MutexLock lock(&devices_by_path_lock_);
  if ((now - last_cleanup_) < kLostPeripheralsCleanupMinFreq) {
    return;
  }
  last_cleanup_ = now;

  for (auto it = devices_by_path_.begin(), end = devices_by_path_.end();
       it != end;) {
    auto copy = it++;
    if (copy->second->Lost()) devices_by_path_.erase(copy);
  }
}

std::shared_ptr<MonitoredBluetoothDevice> BluetoothDevices::add_new_device(
    sdbus::ObjectPath device_object_path) {
  absl::MutexLock l(&devices_by_path_lock_);
  auto [device_it, inserted] = devices_by_path_.emplace(
      std::string(device_object_path),
      std::make_shared<MonitoredBluetoothDevice>(
          system_bus_, std::move(device_object_path), observers_));
  if (!inserted) device_it->second->UnmarkLost();
  return device_it->second;
}
}  // namespace linux
}  // namespace nearby
