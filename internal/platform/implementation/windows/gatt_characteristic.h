// Copyright 2022 Google LLC
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

#ifndef PLATFORM_IMPL_WINDOWS_GATT_CHARACTERISTIC_H_
#define PLATFORM_IMPL_WINDOWS_GATT_CHARACTERISTIC_H_

#include <cstdint>
#include <vector>

#include "absl/time/clock.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"

namespace location {
namespace nearby {
namespace windows {

class GattServer;

// Provides the operations that can be performed on the Gatt Server.
class GattCharacteristic : public nearby::api::ble_v2::GattCharacteristic {
 public:
  explicit GattCharacteristic(
      location::nearby::windows::GattServer& gatt_server, Uuid service_uuid,
      Uuid characteristic_uuid,
      const std::vector<
          location::nearby::api::ble_v2::GattCharacteristic::Permission>&
          permissions,
      const std::vector<
          location::nearby::api::ble_v2::GattCharacteristic::Property>&
          properties);

  ~GattCharacteristic() = default;

 private:
  winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
      GattLocalCharacteristic gatt_characteristic_ = nullptr;

  ByteArray current_value_;

  winrt::Windows::Foundation::Collections::IVectorView<
      winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattSubscribedClient>
      subscribed_clients_;

  winrt::fire_and_forget HandleReadEvent(
      winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattLocalCharacteristic const&,
      winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattReadRequestedEventArgs args);

  winrt::fire_and_forget HandleWriteEvent(
      winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattLocalCharacteristic const&,
      winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattWriteRequestedEventArgs args);

  winrt::fire_and_forget HandleSubscribedClientsChangedEvent(
      winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattLocalCharacteristic const&,
      winrt::Windows::Foundation::IInspectable const& args);
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif
