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

#ifndef PLATFORM_IMPL_LINUX_BLUEZ_GATT_CHARACTERISTIC_CLIENT_H_
#define PLATFORM_IMPL_LINUX_BLUEZ_GATT_CHARACTERISTIC_CLIENT_H_

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/StandardInterfaces.h>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/gatt_characteristic_client.h"
namespace nearby {
namespace linux {
namespace bluez {
class GattCharacteristicClient
    : public sdbus::ProxyInterfaces<org::bluez::GattCharacteristic1_proxy,
                                    sdbus::Properties_proxy> {
 public:
  GattCharacteristicClient(std::shared_ptr<sdbus::IConnection> system_bus,
                           sdbus::ObjectPath path)
      : ProxyInterfaces(*system_bus, "org.bluez", std::move(path)),
        system_bus_(std::move(system_bus)) {
    registerProxy();
  }
  virtual ~GattCharacteristicClient() { unregisterProxy(); }

 protected:
  void onPropertiesChanged(
      const std::string& interfaceName,
      const std::map<std::string, sdbus::Variant>& changedProperties,
      const std::vector<std::string>& invalidatedProperties) override {}

  std::shared_ptr<sdbus::IConnection> system_bus_;
};

class SubscribedGattCharacteristicClient : public GattCharacteristicClient {
 public:
  SubscribedGattCharacteristicClient(
      std::shared_ptr<sdbus::IConnection> system_bus, sdbus::ObjectPath path,
      absl::AnyInvocable<void(absl::string_view value)> notify_callback)
      : GattCharacteristicClient(std::move(system_bus), std::move(path)),
        notify_callback_(std::move(notify_callback)) {}

 protected:
  void onPropertiesChanged(
      const std::string& interfaceName,
      const std::map<std::string, sdbus::Variant>& changedProperties,
      const std::vector<std::string>& invalidatedProperties) override;

 private:
  absl::AnyInvocable<void(absl::string_view value)> notify_callback_;
};
}  // namespace bluez
}  // namespace linux
}  // namespace nearby

#endif
