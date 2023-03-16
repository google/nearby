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
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/base.h"

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

  absl::optional<std::string> ReadCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic) override;

  bool WriteCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic,
      absl::string_view value,
      api::ble_v2::GattClient::WriteType write_type) override;

  bool SetCharacteristicSubscription(
      const api::ble_v2::GattCharacteristic& characteristic, bool enable,
      absl::AnyInvocable<void(absl::string_view value)>
          on_characteristic_changed_cb) override;

  void Disconnect() override;

 private:
  // Used to save native data related to the GATT characteristic.
  struct GattCharacteristicData {
    std::optional<::winrt::Windows::Devices::Bluetooth::
                      GenericAttributeProfile::GattCharacteristic>
        native_characteristic = std::nullopt;
    ::winrt::event_token notification_token;
    absl::AnyInvocable<void(absl::string_view value)>
        on_characteristic_changed_cb;
  };
  std::optional<::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
                    GattCharacteristic>
  GetNativeCharacteristic(const Uuid& service_uuid,
                          const Uuid& characteristic_uuid);

  bool WriteCharacteristicConfigurationDescriptor(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCharacteristic& characteristic,
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattClientCharacteristicConfigurationDescriptorValue value);

  void OnCharacteristicValueChanged(
      const api::ble_v2::GattCharacteristic& characteristic,
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattValueChangedEventArgs args);

  absl::Mutex mutex_;

  ::winrt::Windows::Devices::Bluetooth::BluetoothLEDevice ble_device_;
  ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
      GattDeviceServicesResult gatt_devices_services_result_ = nullptr;

  absl::flat_hash_map<api::ble_v2::GattCharacteristic, GattCharacteristicData>
      native_characteristic_map_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_GATT_CLIENT_H_
