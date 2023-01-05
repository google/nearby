
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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_IMPL_WINDOWS_BLE_GATT_CLIENT_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_IMPL_WINDOWS_BLE_GATT_CLIENT_H_

#include <guiddef.h>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "internal/platform/byte_array.h"
#include "winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "winrt/Windows.Devices.Bluetooth.h"

namespace nearby {
namespace windows {

class BleGattClient {
 public:
  using GattServiceList =
      std::vector<::winrt::Windows::Devices::Bluetooth::
                      GenericAttributeProfile::GattDeviceService>;
  using GattCharacteristicList =
      std::vector<::winrt::Windows::Devices::Bluetooth::
                      GenericAttributeProfile::GattCharacteristic>;
  using GattDescriptorList =
      std::vector<::winrt::Windows::Devices::Bluetooth::
                      GenericAttributeProfile::GattDescriptor>;

  explicit BleGattClient(
      ::winrt::Windows::Devices::Bluetooth::BluetoothLEDevice& ble_device);

  BleGattClient(const BleGattClient&) = delete;
  BleGattClient& operator=(const BleGattClient&) = delete;

  ~BleGattClient() = default;

  void StartGattDiscovery();

  absl::optional<::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
                     GattCharacteristic>
  GetCharacteristic(std::string_view service_uuid,
                    std::string_view characteristic_uuid);
  GattCharacteristicList& GetCharacteristics(std::string_view service_uuid);
  bool WriteCharacteristic(std::string_view service_uuid,
                           std::string_view characteristic_uuid,
                           const ByteArray& value);
  bool WriteCharacteristic(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCharacteristic& characteristics,
      const ByteArray& value);

 private:
  void OnGetGattServices(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattDeviceServicesResult& services_result);

  void OnServiceOpen(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattDeviceService& gatt_service,
      uint16_t service_attribute_handle,
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattOpenStatus open_status);

  void OnGetCharacteristics(
      uint16_t service_attribute_handle,
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCharacteristicsResult& characteristics_result);

  bool OnWriteValueWithResultAndOption(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattWriteResult& write_result);

  void SubscribeToNotifications(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCharacteristic& characteristics);

  void UnsubscribeFromNotifications(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCharacteristic& characteristics);

  bool WriteCccDescriptor(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCharacteristic& characteristics,
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattClientCharacteristicConfigurationDescriptorValue value);

  void AddValueChangedHandler(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCharacteristic& characteristics);

  void RemoveValueChangedHandler(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCharacteristic& characteristics);

  void OnValueChanged(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCharacteristic const& characteristic,
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattValueChangedEventArgs args);

  ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
      GattClientCharacteristicConfigurationDescriptorValue
      GetCccdValue(
          ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCharacteristicProperties properties);

  ::winrt::Windows::Devices::Bluetooth::BluetoothLEDevice ble_device_;
  ::winrt::event_token notifications_token_;
  bool service_discovered_ = false;

  GattServiceList gatt_services_;
  absl::flat_hash_map<uint16_t, GattCharacteristicList>
      service_to_characteristics_map_;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_IMPL_WINDOWS_BLE_GATT_CLIENT_H_
