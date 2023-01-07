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
#include "internal/platform/implementation/windows/gatt_server.h"

#include <exception>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/windows/gatt_characteristic.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.Advertisement.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {

// Specifies common Bluetooth error cases.
// https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetootherror?view=winrt-22621
using winrt::Windows::Devices::Bluetooth::BluetoothError;

// This class is the result of the CreateAsync operation.
// https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.genericattributeprofile.gattserviceproviderresult?view=winrt-22621
using winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattServiceProviderResult;

// The following should be centralized, to be shared across layers
// The most significant bits and the least significant bits for the Copresence
// UUID.
constexpr std::uint64_t kCopresenceServiceUuidMsb = 0x0000FEF300001000;
constexpr std::uint64_t kCopresenceServiceUuidLsb = 0x800000805F9B34FB;

ABSL_CONST_INIT const Uuid kCopresenceServiceUuid(kCopresenceServiceUuidMsb,
                                                  kCopresenceServiceUuidLsb);

GattServer::GattServer(ServerGattConnectionCallback server_callback)
    : server_callback_(server_callback) {
  winrt::guid rtGuid = winrt::guid(std::string(kCopresenceServiceUuid));

  GattServiceProviderResult result =
      GattServiceProvider::CreateAsync(rtGuid).get();

  if (result.Error() == BluetoothError::Success) {
    service_provider_ = result.ServiceProvider();
  }
}

absl::optional<api::ble_v2::GattCharacteristic>
GattServer::CreateCharacteristic(
    const Uuid& service_uuid, const Uuid& characteristic_uuid,
    const std::vector<nearby::api::ble_v2::GattCharacteristic::Permission>&
        permissions,
    const std::vector<nearby::api::ble_v2::GattCharacteristic::Property>&
        properties) {
  return windows::GattCharacteristic(*this, service_uuid, characteristic_uuid,
                                     permissions, properties);
}

// https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#setValue(byte[])
//
// Locally updates the value of a characteristic and returns whether or not
// it was successful.
bool GattServer::UpdateCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    const nearby::ByteArray& value) {
  value_ = value;
  return true;
}

// Stops a GATT server.
void GattServer::Stop() {}

}  // namespace windows
}  // namespace nearby
