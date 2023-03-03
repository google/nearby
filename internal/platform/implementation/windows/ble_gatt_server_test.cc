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

#include "internal/platform/implementation/windows/ble_gatt_server.h"

#include <string>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"

namespace nearby {
namespace windows {
namespace {

TEST(BleV2GattServer, DISABLED_StartStopAdvertising) {
  BluetoothAdapter bluetoothAdapter;
  BleGattServer blev2_gatt_server(&bluetoothAdapter, {});

  ByteArray
      service_data;  // {.tx_power_level = api::ble_v2::TxPowerLevel::kHigh}

  EXPECT_TRUE(blev2_gatt_server.StartAdvertisement(service_data, true));

  EXPECT_TRUE(blev2_gatt_server.StopAdvertisement());
}

TEST(BleV2GattServer, DISABLED_Stop) {
  BluetoothAdapter bluetoothAdapter;
  BleGattServer blev2_gatt_server(&bluetoothAdapter, {});

  blev2_gatt_server.Stop();
}

TEST(BleV2GattServer, DISABLED_CreateCharacteristic) {
  BluetoothAdapter bluetoothAdapter;
  BleGattServer blev2_gatt_server(&bluetoothAdapter, {});

  Uuid service_uuid;
  Uuid characteristic_uuid;
  const api::ble_v2::GattCharacteristic::Permission permissions =
      api::ble_v2::GattCharacteristic::Permission::kNone;
  const api::ble_v2::GattCharacteristic::Property properties =
      api::ble_v2::GattCharacteristic::Property::kNone;

  EXPECT_TRUE(blev2_gatt_server
                  .CreateCharacteristic(service_uuid, characteristic_uuid,
                                        permissions, properties)
                  .has_value());
}

TEST(BleV2GattServer, DISABLED_UpdateCharacteristic) {
  BluetoothAdapter bluetoothAdapter;
  BleGattServer blev2_gatt_server(&bluetoothAdapter, {});

  Uuid service_uuid;
  Uuid characteristic_uuid;
  const api::ble_v2::GattCharacteristic::Permission permissions =
      api::ble_v2::GattCharacteristic::Permission::kNone;
  const api::ble_v2::GattCharacteristic::Property properties =
      api::ble_v2::GattCharacteristic::Property::kNone;

  auto gatt_characteristic = blev2_gatt_server.CreateCharacteristic(
      service_uuid, characteristic_uuid, permissions, properties);
  ByteArray value;

  EXPECT_TRUE(blev2_gatt_server.UpdateCharacteristic(
      gatt_characteristic.value(), value));
}

}  // namespace
}  // namespace windows
}  // namespace nearby
