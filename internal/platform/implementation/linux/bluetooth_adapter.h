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

#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_ADAPTER_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_ADAPTER_H_
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/ProxyInterfaces.h>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/adapter_client.h"

namespace nearby {
namespace linux {
class BluezAdapter : public sdbus::ProxyInterfaces<org::bluez::Adapter1_proxy> {
 public:
  BluezAdapter(sdbus::IConnection &system_bus,
               const sdbus::ObjectPath &adapter_object_path)
      : ProxyInterfaces(system_bus, bluez::SERVICE_DEST, adapter_object_path) {
    registerProxy();
  }
  ~BluezAdapter() { unregisterProxy(); }
};

class BluetoothAdapter : public api::BluetoothAdapter {
 public:
  BluetoothAdapter(std::shared_ptr<sdbus::IConnection> system_bus,
                   const sdbus::ObjectPath &adapter_object_path)
      : system_bus_(std::move(system_bus)),
        bluez_adapter_(std::make_shared<BluezAdapter>(*system_bus_,
                                                      adapter_object_path)) {}

  ~BluetoothAdapter() override = default;

  bool SetStatus(Status status) override;
  bool IsEnabled() const override;

  ScanMode GetScanMode() const override;

  bool SetScanMode(ScanMode scan_mode) override;
  std::string GetName() const override;

  bool SetName(absl::string_view name) override;
  bool SetName(absl::string_view name, bool persist) override;
  std::string GetMacAddress() const override;

  bool RemoveDeviceByObjectPath(const sdbus::ObjectPath &device_object_path) {
    try {
      bluez_adapter_->RemoveDevice(device_object_path);
      return true;
    } catch (const sdbus::Error &e) {
      DBUS_LOG_METHOD_CALL_ERROR(bluez_adapter_, "RemoveDevice", e);
      return false;
    }
  }

  sdbus::ObjectPath GetObjectPath() const {
    return bluez_adapter_->getObjectPath();
  }

  BluezAdapter &GetBluezAdapterObject() { return *bluez_adapter_; }
  std::shared_ptr<sdbus::IConnection> GetConnection() { return system_bus_; }

 private:
  std::shared_ptr<sdbus::IConnection> system_bus_;
  std::shared_ptr<BluezAdapter> bluez_adapter_;
};
}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_BLUETOOTH_ADAPTER_H_
