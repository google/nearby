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

#include <algorithm>
#include <chrono>
#include <functional>
#include <optional>

#include <sdbus-c++/Types.h>

#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_devices.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/device_client.h"
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
    LOG(ERROR) << __func__ << ": Device " << device_object_path
                       << " doesn't exist";
    return;
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
          system_bus_,
          std::make_shared<bluez::Device>(system_bus_, device_object_path),
          observers_));
  if (!inserted) device_it->second->UnmarkLost();
  return device_it->second;
}

void DeviceWatcher::onInterfacesAdded(
    const sdbus::ObjectPath &object,
    const std::map<std::string, std::map<std::string, sdbus::Variant>>
        &interfaces) {
  auto path_prefix = absl::Substitute("$0/dev_", adapter_object_path_);
  if (object.find(path_prefix) != 0) {
    return;
  }

  if (interfaces.count(org::bluez::Device1_proxy::INTERFACE_NAME) == 0) return;

  auto device = devices_->add_new_device(object);
  device->SetDiscoveryCallback(discovery_cb_);
  if (discovery_cb_ != nullptr &&
      discovery_cb_->device_discovered_cb != nullptr) {
    discovery_cb_->device_discovered_cb(*device);
  }

  if (observers_ != nullptr) {
    for (const auto &observer : observers_->GetObservers()) {
      observer->DeviceAdded(*device);
    }
  }
}

void DeviceWatcher::onInterfacesRemoved(
    const sdbus::ObjectPath &object,
    const std::vector<std::string> &interfaces) {
  auto path_prefix = absl::Substitute("$0/dev_", adapter_object_path_);
  if (object.find(path_prefix) != 0) {
    return;
  }

  auto removed_device_it = std::find(interfaces.begin(), interfaces.end(),
                                     org::bluez::Device1_proxy::INTERFACE_NAME);
  if (removed_device_it != interfaces.end()) {
    auto device = devices_->get_device_by_path(object);
    if (device == nullptr) {
      LOG(WARNING) << __func__
                           << ": received InterfacesRemoved for a device "
                              "we don't know about: "
                           << object;
      return;
    }

    LOG(INFO) << __func__ << ": Device " << object
                      << " has been removed";
    if (discovery_cb_ != nullptr && discovery_cb_->device_lost_cb != nullptr) {
      discovery_cb_->device_lost_cb(*device);
    }

    if (observers_ != nullptr) {
      for (const auto &observer : observers_->GetObservers()) {
        observer->DeviceRemoved(*device);
      }
      devices_->remove_device_by_path(object);
    } else {
      devices_->mark_peripheral_lost(object);
    }
  }
}

void DeviceWatcher::notifyExistingDevices() {
  std::map<sdbus::ObjectPath,
           std::map<std::string, std::map<std::string, sdbus::Variant>>>
      objects;
  try {
    objects = GetManagedObjects();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(this, "GetManagedObjects", e);
    return;
  }

  std::vector<sdbus::ObjectPath> existing_device_paths;

  for (const auto& [device_path, interfaces] : objects) {
    if (device_path.find(absl::Substitute("$0/dev_", adapter_object_path_)) == 0 &&
        interfaces.count(org::bluez::Device1_proxy::INTERFACE_NAME) == 1) {
      
      // Don't remove bonded, paired, connected, or trusted devices
      bool should_skip = false;
      auto device_interface_it = interfaces.find(org::bluez::Device1_proxy::INTERFACE_NAME);
      if (device_interface_it != interfaces.end()) {
        const auto& properties = device_interface_it->second;
        
        auto check_bool_property = [&properties](const std::string& prop_name) -> bool {
          auto it = properties.find(prop_name);
          if (it != properties.end()) {
            try {
              return it->second.get<bool>();
            } catch (...) {
              return false;
            }
          }
          return false;
        };
        
        if (check_bool_property("Bonded") || 
            check_bool_property("Paired") ||
            check_bool_property("Connected") ||
            check_bool_property("Trusted")) {
          should_skip = true;
          LOG(INFO) << __func__ << ": Skipping device " << device_path 
                    << " (bonded/paired/connected/trusted)";
        }
      }
      
      if (!should_skip) {
        existing_device_paths.push_back(device_path);
      }
    }
  }

  // Remove existing devices - they will be immediately re-discovered
  // This triggers InterfacesAdded signals which properly invoke discovery callbacks
  for (const auto& device_path : existing_device_paths) {
    LOG(INFO) << __func__ << ": Refreshing existing device " << device_path;
    if (!adapter_.RemoveDeviceByObjectPath(device_path)) {
      LOG(WARNING) << __func__ << ": Failed to remove device " << device_path;
    }
  }
}

}  // namespace linux
}  // namespace nearby
