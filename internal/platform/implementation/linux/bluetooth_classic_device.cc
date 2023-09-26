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

#include <memory>

#include <sdbus-c++/IObject.h>
#include <sdbus-c++/ProxyInterfaces.h>

#include "absl/strings/string_view.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/bluez_device.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
BluetoothDevice::BluetoothDevice(std::shared_ptr<bluez::Device> device)
    : lost_(false), device_(device) {
  try {
    last_known_name_ = device->Alias();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(device, "Alias", e);
  }
  try {
    last_known_address_ = device->Address();
    unique_id_ = BluetoothUtils::ToNumber(last_known_address_);
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(device, "Address", e);
  }
}

std::string BluetoothDevice::GetName() const {
  auto device = device_.lock();
  if (device == nullptr) {
    absl::ReaderMutexLock l(&properties_mutex_);
    return last_known_name_;
  }

  try {
    std::string alias = device->Alias();
    {
      absl::MutexLock l(&properties_mutex_);
      last_known_name_ = alias;
    }
    return alias;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(device, "Alias", e);
    return {};
  }
}

std::string BluetoothDevice::GetMacAddress() const {
  auto device = device_.lock();
  if (device == nullptr) {
    absl::ReaderMutexLock l(&properties_mutex_);
    return last_known_name_;
  }

  try {
    std::string addr = device->Address();
    {
      absl::MutexLock l(&properties_mutex_);
      last_known_address_ = addr;
    }
    return addr;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(device, "Address", e);
    return std::string();
  }
}

bool BluetoothDevice::ConnectToProfile(absl::string_view service_uuid) {
  auto device = device_.lock();
  if (device == nullptr) return false;
  try {
    device->ConnectProfile(std::string(service_uuid));
    return true;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(device, "ConnectProfile", e);
    return false;
  }
}

MonitoredBluetoothDevice::MonitoredBluetoothDevice(
    std::shared_ptr<sdbus::IConnection> system_bus,
    std::shared_ptr<bluez::Device> device,
    ObserverList<api::BluetoothClassicMedium::Observer> &observers)
    : BluetoothDevice(std::move(device)),
      ProxyInterfaces<sdbus::Properties_proxy>(*system_bus, bluez::SERVICE_DEST,
                                               device->getObjectPath()),
      system_bus_(std::move(system_bus)),
      observers_(observers) {
  registerProxy();
}

void MonitoredBluetoothDevice::onPropertiesChanged(
    const std::string &interfaceName,
    const std::map<std::string, sdbus::Variant> &changedProperties,
    const std::vector<std::string> &invalidatedProperties) {
  if (interfaceName != bluez::DEVICE_INTERFACE) {
    return;
  }

  for (auto it = changedProperties.begin(); it != changedProperties.end();
       it++) {
    if (it->first == bluez::DEVICE_PROP_ADDRESS) {
      NEARBY_LOGS(VERBOSE) << __func__ << ": " << getObjectPath()
                           << ": Notifying observers about address change";
      std::string address = it->second;
      for (const auto &observer : observers_.GetObservers()) {
        observer->DeviceAddressChanged(*this, address);
      }
    } else if (it->first == bluez::DEVICE_PROP_PAIRED) {
      NEARBY_LOGS(VERBOSE) << __func__ << ": " << getObjectPath()
                           << "Notifying observers about paired status change.";
      for (const auto &observer : observers_.GetObservers()) {
        observer->DevicePairedChanged(*this, it->second);
      }
    } else if (it->first == bluez::DEVICE_PROP_CONNECTED) {
      NEARBY_LOGS(VERBOSE)
          << __func__ << ": " << getObjectPath()
          << "Notifying observers about connected status change";
      for (const auto &observer : observers_.GetObservers()) {
        observer->DeviceConnectedStateChanged(*this, it->second);
      }
    } else if (it->first == bluez::DEVICE_NAME) {
      auto callback = GetDiscoveryCallback();
      if (callback != nullptr && callback->device_name_changed_cb != nullptr)
        callback->device_name_changed_cb(*this);
    }
  }
}

}  // namespace linux
}  // namespace nearby
