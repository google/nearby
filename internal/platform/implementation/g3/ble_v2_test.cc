// Copyright 2025 Google LLC
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

#include "internal/platform/implementation/g3/ble_v2.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/g3/bluetooth_adapter.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace g3 {
namespace {

using ::testing::Return;

constexpr absl::string_view kTestUuid = "00000000-0000-0000-0000-000000000000";
constexpr absl::string_view kTestServiceId = "test_service_id";

class MockBluetoothAdapter : public BluetoothAdapter {
 public:
  MOCK_METHOD(bool, IsValid, (), (const));
  MOCK_METHOD(bool, SetStatus, (api::BluetoothAdapter::Status status),
              (override));
  MOCK_METHOD(bool, IsEnabled, (), (const, override));
  MOCK_METHOD(api::BluetoothAdapter::Status, GetStatus, (), (const));
  MOCK_METHOD(bool, SetName, (absl::string_view name), (override));
  MOCK_METHOD(bool, SetName, (absl::string_view name, bool persist),
              (override));
  MOCK_METHOD(std::string, GetName, (), (const, override));
  MOCK_METHOD(std::string, GetMacAddress, (), (const, override));
  MOCK_METHOD(bool, SetScanMode, (api::BluetoothAdapter::ScanMode scan_mode),
              (override));
  MOCK_METHOD(api::BluetoothAdapter::ScanMode, GetScanMode, (),
              (const, override));
  MOCK_METHOD(int, GetNumSlots, (), (const));
  MOCK_METHOD(api::ble_v2::BlePeripheral::UniqueId, GetUniqueId, (), (const));
  MOCK_METHOD(void, SetBleV2Medium, (api::ble_v2::BleMedium * medium));
};

TEST(BleV2MediumTest, StartAdvertising) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleV2Medium ble_v2_medium(mock_bluetooth_adapter);
  api::ble_v2::BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = false;
  advertising_data.service_data.insert(
      {Uuid(kTestUuid), ByteArray("test_service_data")});

  auto advertising_session = ble_v2_medium.StartAdvertising(
      advertising_data,
      {.tx_power_level = api::ble_v2::TxPowerLevel::kLow,
       .is_connectable = true},
      api::ble_v2::BleMedium::AdvertisingCallback{
          .start_advertising_result = [](absl::Status) {},
      });
  EXPECT_TRUE(advertising_session != nullptr);
}

TEST(BleV2MediumTest, StopAdvertising) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleV2Medium ble_v2_medium(mock_bluetooth_adapter);
  EXPECT_TRUE(ble_v2_medium.StopAdvertising());
}

TEST(BleV2MediumTest, StartScanning) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleV2Medium ble_v2_medium(mock_bluetooth_adapter);
  auto scanning_session = ble_v2_medium.StartScanning(
      Uuid(kTestUuid), api::ble_v2::TxPowerLevel::kLow,
      api::ble_v2::BleMedium::ScanningCallback{
          .start_scanning_result = [](absl::Status) {},
          .advertisement_found_cb = [](api::ble_v2::BlePeripheral::UniqueId,
                                       api::ble_v2::BleAdvertisementData) {},
      });
  EXPECT_TRUE(scanning_session != nullptr);
}

TEST(BleV2MediumTest, StopScanning) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleV2Medium ble_v2_medium(mock_bluetooth_adapter);
  EXPECT_TRUE(ble_v2_medium.StopScanning());
}

TEST(BleV2MediumTest, StartGattServer) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleV2Medium ble_v2_medium(mock_bluetooth_adapter);
  auto gatt_server = ble_v2_medium.StartGattServer({});
  EXPECT_TRUE(gatt_server != nullptr);
}

TEST(BleV2MediumTest, ConnectToGattServer) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleV2Medium ble_v2_medium(mock_bluetooth_adapter);
  auto gatt_client = ble_v2_medium.ConnectToGattServer(
      /*peripheral_id=*/1234, api::ble_v2::TxPowerLevel::kLow, {});
  EXPECT_TRUE(gatt_client == nullptr);
}

TEST(BleV2MediumTest, OpenServerSocket) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleV2Medium ble_v2_medium(mock_bluetooth_adapter);
  auto server_socket =
      ble_v2_medium.OpenServerSocket(std::string(kTestServiceId));
  EXPECT_TRUE(server_socket != nullptr);
}

TEST(BleV2MediumTest, Connect) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleV2Medium ble_v2_medium(mock_bluetooth_adapter);
  auto socket = ble_v2_medium.Connect(
      std::string(kTestServiceId), api::ble_v2::TxPowerLevel::kLow,
      /*remote_peripheral_id=*/1234, /*cancellation_flag=*/nullptr);
  EXPECT_TRUE(socket == nullptr);
}

TEST(BleV2MediumTest, OpenL2capServerSocket) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleV2Medium ble_v2_medium(mock_bluetooth_adapter);
  auto server_socket =
      ble_v2_medium.OpenL2capServerSocket(std::string(kTestServiceId));
  EXPECT_TRUE(server_socket == nullptr);
}

TEST(BleV2MediumTest, IsExtendedAdvertisementsAvailable) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleV2Medium ble_v2_medium(mock_bluetooth_adapter);
  EXPECT_FALSE(ble_v2_medium.IsExtendedAdvertisementsAvailable());
}

}  // namespace
}  // namespace g3
}  // namespace nearby
