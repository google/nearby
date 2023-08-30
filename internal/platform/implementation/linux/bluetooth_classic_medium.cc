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

#include <cstring>
#include <memory>

#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>

#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_bluez_profile.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_classic_medium.h"
#include "internal/platform/implementation/linux/bluetooth_classic_server_socket.h"
#include "internal/platform/implementation/linux/bluetooth_classic_socket.h"
#include "internal/platform/implementation/linux/bluetooth_pairing.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/device_client.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
BluetoothClassicMedium::BluetoothClassicMedium(
    sdbus::IConnection &system_bus,
    const sdbus::ObjectPath &adapter_object_path)
    : ProxyInterfaces(system_bus, "org.bluez", "/"),
      adapter_(
          std::make_unique<BluetoothAdapter>(system_bus, adapter_object_path)),
      devices_(std::make_unique<BluetoothDevices>(
          system_bus, adapter_object_path, observers_)),
      profile_manager_(
          std::make_unique<ProfileManager>(system_bus, *devices_)) {
  registerProxy();
}

void BluetoothClassicMedium::onInterfacesAdded(
    const sdbus::ObjectPath &object,
    const std::map<std::string, std::map<std::string, sdbus::Variant>>
        &interfaces) {
  auto path_prefix = absl::Substitute(
      "$0/dev_", adapter_->GetBluezAdapterObject().getObjectPath());
  if (object.find(path_prefix) != 0) {
    return;
  }

  if (devices_->get_device_by_path(object).has_value()) {
    // Device already exists.
    return;
  }

  if (interfaces.count(org::bluez::Device1_proxy::INTERFACE_NAME) == 1) {
    NEARBY_LOGS(INFO) << __func__ << ": Encountered new device at " << object;

    auto &device = devices_->add_new_device(object);

    if (discovery_cb_.has_value() &&
        discovery_cb_->device_discovered_cb != nullptr) {
      discovery_cb_->device_discovered_cb(device);
    }

    for (const auto &observer : observers_.GetObservers()) {
      observer->DeviceAdded(device);
    }
  }
}

void BluetoothClassicMedium::onInterfacesRemoved(
    const sdbus::ObjectPath &object,
    const std::vector<std::string> &interfaces) {
  auto path_prefix = absl::Substitute("$0/dev_", adapter_->GetObjectPath());
  if (object.find(path_prefix) != 0) {
    return;
  }

  for (const auto &interface : interfaces) {
    if (interface == org::bluez::Device1_proxy::INTERFACE_NAME) {
      {
        auto device = devices_->get_device_by_path(object);
        if (!device.has_value()) {
          NEARBY_LOGS(WARNING) << __func__
                               << ": received InterfacesRemoved for a device "
                                  "we don't know about: "
                               << object;
          return;
        }

        NEARBY_LOGS(INFO) << __func__ << ": Device " << object
                          << " has been removed";
        if (discovery_cb_.has_value() &&
            discovery_cb_->device_lost_cb != nullptr) {
          discovery_cb_->device_lost_cb(*device);
        }

        for (const auto &observer : observers_.GetObservers()) {
          observer->DeviceRemoved(*device);
        }
      }
      devices_->remove_device_by_path(object);
    }
  }
}

bool BluetoothClassicMedium::StartDiscovery(
    DiscoveryCallback discovery_callback) {
  discovery_cb_ = std::move(discovery_callback);

  try {
    NEARBY_LOGS(INFO) << __func__ << ": Starting discovery on "
                      << adapter_->GetObjectPath();
    adapter_->GetBluezAdapterObject().StartDiscovery();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(&adapter_->GetBluezAdapterObject(),
                               "StartDiscovery", e);
    discovery_cb_.reset();
    return false;
  }

  return true;
}

bool BluetoothClassicMedium::StopDiscovery() {
  auto &adapter = adapter_->GetBluezAdapterObject();

  try {
    NEARBY_LOGS(INFO) << __func__ << "Stopping discovery on "
                      << adapter.getObjectPath();

    adapter.StopDiscovery();
    this->discovery_cb_.reset();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(&adapter, "StopDiscovery", e);
    return false;
  }

  return true;
}

std::unique_ptr<api::BluetoothSocket> BluetoothClassicMedium::ConnectToService(
    api::BluetoothDevice &remote_device, const std::string &service_uuid,
    CancellationFlag *cancellation_flag) {
  auto device_object_path = bluez::device_object_path(
      adapter_->GetObjectPath(), remote_device.GetMacAddress());
  if (!profile_manager_->ProfileRegistered(service_uuid)) {
    if (!profile_manager_->Register("", service_uuid)) {
      NEARBY_LOGS(ERROR) << __func__ << ": Could not register profile "
                         << service_uuid << " with Bluez";
      return nullptr;
    }
  }

  auto maybe_device = devices_->get_device_by_path(device_object_path);
  if (!maybe_device.has_value()) return nullptr;

  auto &device = maybe_device->get();
  device.ConnectToProfile(service_uuid);

  auto fd = profile_manager_->GetServiceRecordFD(remote_device, service_uuid,
                                                 cancellation_flag);
  if (!fd.has_value()) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Failed to get a new connection for profile "
                         << service_uuid << " for device "
                         << device_object_path;
    return nullptr;
  }

  return std::unique_ptr<api::BluetoothSocket>(
      new BluetoothSocket(remote_device, fd.value()));
}

std::unique_ptr<api::BluetoothServerSocket>
BluetoothClassicMedium::ListenForService(const std::string &service_name,
                                         const std::string &service_uuid) {
  if (!profile_manager_->ProfileRegistered(service_uuid)) {
    if (!profile_manager_->Register(service_name, service_uuid)) {
      NEARBY_LOGS(ERROR) << __func__ << ": Could not register profile "
                         << service_name << " " << service_uuid
                         << " with Bluez";
      return nullptr;
    }
  }

  return std::unique_ptr<api::BluetoothServerSocket>(
      new BluetoothServerSocket(*profile_manager_, service_uuid));
}

api::BluetoothDevice *BluetoothClassicMedium::GetRemoteDevice(
    const std::string &mac_address) {
  auto device = devices_->get_device_by_address(mac_address);
  if (!device.has_value()) return nullptr;

  return &(device->get());
}

std::unique_ptr<api::BluetoothPairing> BluetoothClassicMedium::CreatePairing(
    api::BluetoothDevice &remote_device) {
  auto device = devices_->get_device_by_address(remote_device.GetMacAddress());
  if (!device.has_value()) return nullptr;

  return std::unique_ptr<api::BluetoothPairing>(
      new BluetoothPairing(*adapter_, *device));
}

}  // namespace linux
}  // namespace nearby
