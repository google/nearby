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

#include <map>
#include <string>
#include <vector>

#include "internal/platform/implementation/linux/bluez_gatt_characteristic_client.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/gatt_characteristic_client.h"
namespace nearby {
namespace linux {
namespace bluez {
void SubscribedGattCharacteristicClient::onPropertiesChanged(
    const std::string& interfaceName,
    const std::map<std::string, sdbus::Variant>& changedProperties,
    const std::vector<std::string>& invalidatedProperties) {
  if (interfaceName != org::bluez::GattCharacteristic1_proxy::INTERFACE_NAME)
    return;

  if (changedProperties.count("Value") == 1) {
    std::vector<uint8_t> value_bytes = changedProperties.at("Value");
    if (notify_callback_ != nullptr) {
      auto value = std::string(value_bytes.cbegin(), value_bytes.cend());
      notify_callback_(value);
    }
  }
}
}  // namespace bluez
}  // namespace linux
}  // namespace nearby
