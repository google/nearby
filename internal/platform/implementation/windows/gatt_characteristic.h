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

namespace nearby {
namespace windows {

// This class represents a local characteristic.
// https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.genericattributeprofile.gattlocalcharacteristic?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattLocalCharacteristic;

// This class contains the arguments for the StateChanged event.
// https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.genericattributeprofile.gattreadrequestedeventargs?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattReadRequestedEventArgs;

// This class represents the event args for WriteRequested.
// https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.genericattributeprofile.gattwriterequestedeventargs?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteRequestedEventArgs;

// This class represents a subscribed client of a GATT session.
// https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.genericattributeprofile.gattsubscribedclient?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattSubscribedClient;

// Provides functionality required for all Windows Runtime classes.
// The IInspectable interface inherits from the IUnknown interface.
// https://learn.microsoft.com/en-us/windows/win32/api/inspectable/nn-inspectable-iinspectable
using ::winrt::Windows::Foundation::IInspectable;

// Represents an immutable view into a vector.
// https://learn.microsoft.com/en-us/uwp/api/windows.foundation.collections.ivectorview-1?view=winrt-22621
using ::winrt::Windows::Foundation::Collections::IVectorView;

class GattServer;

// Provides the operations that can be performed on the Gatt Server.
class GattCharacteristic : public api::ble_v2::GattCharacteristic {
 public:
  GattCharacteristic(
      GattServer& gatt_server_owner, Uuid desired_service_uuid,
      Uuid desired_characteristic_uuid,
      const std::vector<api::ble_v2::GattCharacteristic::Permission>&
          desired_permissions,
      const std::vector<api::ble_v2::GattCharacteristic::Property>&
          desired_properties);

 private:
  GattLocalCharacteristic gatt_characteristic_ = nullptr;

  GattServer& gatt_server_;
  IVectorView<GattSubscribedClient> subscribed_clients_;

  ByteArray current_value_;

  winrt::fire_and_forget HandleReadEvent(GattLocalCharacteristic const&,
                                         GattReadRequestedEventArgs args);

  winrt::fire_and_forget HandleWriteEvent(GattLocalCharacteristic const&,
                                          GattWriteRequestedEventArgs args);

  winrt::fire_and_forget HandleSubscribedClientsChangedEvent(
      GattLocalCharacteristic const&, IInspectable const& args);
};

}  // namespace windows
}  // namespace nearby

#endif
