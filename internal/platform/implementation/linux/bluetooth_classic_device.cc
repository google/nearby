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

#include <sdbus-c++/IObject.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <systemd/sd-bus.h>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
BluetoothDevice::BluetoothDevice(sdbus::IConnection &system_bus,
                                 sdbus::ObjectPath device_object_path)
    : ProxyInterfaces(system_bus, bluez::SERVICE_DEST,
                      std::move(device_object_path)) {
  registerProxy();
  try {
    last_known_name_ = Alias();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(this, "Alias", e);
  }
  try {
    last_known_address_ = Address();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(this, "Address", e);
  }
}

std::string BluetoothDevice::GetName() const {
  auto bluez_device =
      sdbus::createProxy(getProxy().getConnection(), bluez::SERVICE_DEST,
                         getProxy().getObjectPath());

  try {
    std::string alias =
        bluez_device->getProperty("Alias").onInterface(bluez::DEVICE_INTERFACE);
    {
      absl::MutexLock l(&properties_mutex_);
      last_known_name_ = alias;
    }
    return alias;
  } catch (const sdbus::Error &e) {
    if (e.getName() == "org.freedesktop.DBus.Error.UnknownObject") {
      NEARBY_LOGS(VERBOSE)
          << __func__ << ": " << getObjectPath()
          << ": device is no longer known, returning last known name";
      absl::ReaderMutexLock l(&properties_mutex_);
      return last_known_name_;
    }
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while trying to get Alias for device "
                       << bluez_device->getObjectPath();
    return std::string();
  }
}

std::string BluetoothDevice::GetMacAddress() const {
  auto bluez_device =
      sdbus::createProxy(getProxy().getConnection(), bluez::SERVICE_DEST,
                         getProxy().getObjectPath());

  try {
    std::string addr = bluez_device->getProperty("Address").onInterface(
        bluez::DEVICE_INTERFACE);
    {
      absl::MutexLock l(&properties_mutex_);
      last_known_address_ = addr;
    }
    return addr;
  } catch (const sdbus::Error &e) {
    if (e.getName() == "org.freedesktop.DBus.Error.UnknownObject") {
      NEARBY_LOGS(VERBOSE)
          << __func__ << ": " << getObjectPath()
          << ": device is no longer known, returning last known address";
      absl::ReaderMutexLock l(&properties_mutex_);
      return last_known_address_;
    }

    NEARBY_LOGS(ERROR) << __func__ << "Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while trying to get Address for device "
                       << bluez_device->getObjectPath();
    return std::string();
  }
}

void BluetoothDevice::onConnectProfileReply(const sdbus::Error *error) {
  if (error != nullptr && error->getName() != "org.bluez.Error.InProgress") {
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << error->getName()
                       << "' with message '" << error->getMessage()
                       << " while connecting to profile.";
  }
}

bool BluetoothDevice::ConnectToProfile(absl::string_view service_uuid) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": " << getObjectPath()
                       << ": Attempting to connect to profile " << service_uuid;
  try {
    ConnectProfile(std::string(service_uuid));
    return true;
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while trying to asynchronously connect to profile "
                       << service_uuid << " on device " << getObjectPath();
    return false;
  }
}

MonitoredBluetoothDevice::MonitoredBluetoothDevice(
    sdbus::IConnection &system_bus, const sdbus::ObjectPath &device_object_path,
    ObserverList<api::BluetoothClassicMedium::Observer> &observers)
    : BluetoothDevice(system_bus, device_object_path),
      ProxyInterfaces<sdbus::Properties_proxy>(system_bus, bluez::SERVICE_DEST,
                                               std::string(device_object_path)),
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
      for (auto &observer : observers_.GetObservers()) {
        observer->DeviceAddressChanged(*this, address);
      }
    } else if (it->first == bluez::DEVICE_PROP_PAIRED) {
      NEARBY_LOGS(VERBOSE) << __func__ << ": " << getObjectPath()
                           << "Notifying observers about paired status change.";
      for (auto &observer : observers_.GetObservers()) {
        observer->DevicePairedChanged(*this, it->second);
      }
    } else if (it->first == bluez::DEVICE_PROP_CONNECTED) {
      NEARBY_LOGS(VERBOSE)
          << __func__ << ": " << getObjectPath()
          << "Notifying observers about connected status change";
      for (auto &observer : observers_.GetObservers()) {
        observer->DeviceConnectedStateChanged(*this, it->second);
      }
    }
  }
}

}  // namespace linux
}  // namespace nearby
