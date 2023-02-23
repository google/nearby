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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_GATT_CLIENT_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_GATT_CLIENT_H_

#include <windows.h>

#include <cstddef>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "winrt/Windows.Devices.Bluetooth.h"

namespace nearby {
namespace windows {

class BleGattClient : public api::ble_v2::GattClient {
 public:
  explicit BleGattClient(
      ::winrt::Windows::Devices::Bluetooth::BluetoothLEDevice ble_device);
  ~BleGattClient() override;

  bool DiscoverServiceAndCharacteristics(
      const Uuid& service_uuid,
      const std::vector<Uuid>& characteristic_uuids) override;

  absl::optional<api::ble_v2::GattCharacteristic> GetCharacteristic(
      const Uuid& service_uuid, const Uuid& characteristic_uuid) override;

  absl::optional<ByteArray> ReadCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic) override;

  bool WriteCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic,
      const ByteArray& value) override;

  void Disconnect() override;

 private:
  std::optional<::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
                    GattCharacteristic>
  GetNativeCharacteristic(const Uuid& service_uuid,
                          const Uuid& characteristic_uuid);

  ::winrt::Windows::Devices::Bluetooth::BluetoothLEDevice ble_device_;
  ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
      GattDeviceServicesResult gatt_devices_services_result_ = nullptr;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_GATT_CLIENT_H_
