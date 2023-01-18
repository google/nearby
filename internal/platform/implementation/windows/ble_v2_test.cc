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

#include "internal/platform/implementation/windows/ble_v2.h"

#include <string>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"

namespace nearby {
namespace windows {
namespace {

TEST(BleV2Medium, DISABLED_StartAdvertising) {
  BluetoothAdapter bluetoothAdapter;
  BleV2Medium blev2_medium(bluetoothAdapter);

  api::ble_v2::BleAdvertisementData advertising_data;
  api::ble_v2::BleAdvertisementData scan_response_data;

  EXPECT_TRUE(blev2_medium.StartAdvertising(
      advertising_data, {.tx_power_level = api::ble_v2::TxPowerLevel::kHigh,
                         .is_connectable = true}));
}

TEST(BleV2Medium, DISABLED_StopAdvertising) {
  BluetoothAdapter bluetoothAdapter;
  BleV2Medium blev2_medium(bluetoothAdapter);

  api::ble_v2::BleAdvertisementData advertising_data;

  EXPECT_TRUE(blev2_medium.StartAdvertising(
      advertising_data, {.tx_power_level = api::ble_v2::TxPowerLevel::kHigh,
                         .is_connectable = true}));
  EXPECT_TRUE(blev2_medium.StopAdvertising());
}

TEST(BleV2Medium, DISABLED_StartScanning) {
  bool scan_response_received = false;
  absl::Notification scan_response_notification;

  BluetoothAdapter bluetoothAdapter;
  BleV2Medium blev2_medium(bluetoothAdapter);
  Uuid service_uuid;

  api::ble_v2::BleMedium::ScanCallback callback;
  callback.advertisement_found_cb =
      [&scan_response_received, &scan_response_notification](
          api::ble_v2::BlePeripheral& peripheral,
          const api::ble_v2::BleAdvertisementData& advertisement_data) {
        scan_response_received = true;
        scan_response_notification.Notify();
      };

  EXPECT_TRUE(blev2_medium.StartScanning(
      service_uuid, api::ble_v2::TxPowerLevel::kHigh, std::move(callback)));

  EXPECT_TRUE(scan_response_notification.WaitForNotificationWithTimeout(
      absl::Seconds(5)));
  EXPECT_TRUE(scan_response_received);
}

TEST(BleV2Medium, DISABLED_StopScanning) {
  BluetoothAdapter bluetoothAdapter;
  BleV2Medium blev2_medium(bluetoothAdapter);
  Uuid service_uuid;

  api::ble_v2::BleMedium::ScanCallback callback;
  callback.advertisement_found_cb =
      [](api::ble_v2::BlePeripheral& peripheral,
         const api::ble_v2::BleAdvertisementData& advertisement_data) {};

  EXPECT_TRUE(blev2_medium.StartScanning(
      service_uuid, api::ble_v2::TxPowerLevel::kHigh, std::move(callback)));

  EXPECT_TRUE(blev2_medium.StopScanning());
}

}  // namespace
}  // namespace windows
}  // namespace nearby
