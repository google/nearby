// Copyright 2020 Google LLC
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

#include "internal/platform/ble_v2.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"

namespace location {
namespace nearby {
namespace {

using ::location::nearby::api::ble_v2::BleAdvertisementData;
using ::location::nearby::api::ble_v2::GattCharacteristic;
using ::location::nearby::api::ble_v2::TxPowerLevel;
using ::testing::Optional;

constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kAdvertisementString = "\x0a\x0b\x0c\x0d";
constexpr absl::string_view kAdvertisementHeaderString = "\x0x\x0y\x0z";
constexpr absl::string_view kCopresenceServiceUuid = "F3FE";
constexpr TxPowerLevel kTxPowerLevel(TxPowerLevel::kHigh);

// A stub BlePeripheral implementation.
class BlePeripheralStub : public api::ble_v2::BlePeripheral {
 public:
  explicit BlePeripheralStub(absl::string_view mac_address) {
    mac_address_ = mac_address;
  }

  std::string GetAddress() const override { return mac_address_; }

 private:
  std::string mac_address_;
};

class BleV2MediumTest : public testing::Test {
 protected:
  BleV2MediumTest() { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(BleV2MediumTest, ConstructorDestructorWorks) {
  env_.Start();
  BluetoothAdapter adapter_a;
  BluetoothAdapter adapter_b;
  BleV2Medium ble_a(adapter_a);
  BleV2Medium ble_b(adapter_b);

  // Make sure we can create functional mediums.
  ASSERT_TRUE(ble_a.IsValid());
  ASSERT_TRUE(ble_b.IsValid());

  // Make sure we can create 2 distinct mediums.
  EXPECT_NE(ble_a.GetImpl(), ble_b.GetImpl());
  env_.Stop();
}

TEST_F(BleV2MediumTest, CanStartFastScanningAndFastAdvertising) {
  env_.Start();
  BluetoothAdapter adapter_a;
  BluetoothAdapter adapter_b;
  BleV2Medium ble_a(adapter_a);
  BleV2Medium ble_b(adapter_b);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  CountDownLatch found_latch(1);

  EXPECT_TRUE(ble_a.StartScanning(
      {std::string(kCopresenceServiceUuid)}, kTxPowerLevel,
      {
          .advertisement_found_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const BleAdvertisementData& advertisement_data) {
                found_latch.CountDown();
              },
      }));

  // Fail to start extended advertisement due to g3 Ble medium does not support.
  BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = true;
  advertising_data.service_data.insert(
      {std::string(kCopresenceServiceUuid), advertisement_bytes});
  EXPECT_FALSE(ble_b.StartAdvertising(
      advertising_data,
      {.tx_power_level = kTxPowerLevel, .is_connectable = true}));

  // Succeed to start regular advertisement.
  advertising_data.is_extended_advertisement = false;
  advertising_data.service_data = {
      {std::string(kCopresenceServiceUuid), advertisement_bytes}};
  EXPECT_TRUE(ble_b.StartAdvertising(
      advertising_data,
      {.tx_power_level = kTxPowerLevel, .is_connectable = true}));

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopScanning());
  EXPECT_TRUE(ble_b.StopAdvertising());
  env_.Stop();
}

TEST_F(BleV2MediumTest, CanStartScanningAndAdvertising) {
  env_.Start();
  BluetoothAdapter adapter_a;
  BluetoothAdapter adapter_b;
  BleV2Medium ble_a(adapter_a);
  BleV2Medium ble_b(adapter_b);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  ByteArray advertisement_header_bytes{std::string(kAdvertisementHeaderString)};
  CountDownLatch found_latch(1);

  EXPECT_TRUE(ble_a.StartScanning(
      {std::string(kCopresenceServiceUuid)}, kTxPowerLevel,
      {
          .advertisement_found_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const BleAdvertisementData& advertisement_data) {
                found_latch.CountDown();
              },
      }));

  // Fail to start extended advertisement due to g3 Ble medium does not support.
  BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = true;
  advertising_data.service_data.insert(
      {std::string(kCopresenceServiceUuid), advertisement_bytes});
  EXPECT_FALSE(ble_b.StartAdvertising(
      advertising_data,
      {.tx_power_level = kTxPowerLevel, .is_connectable = true}));

  // Succeed to start regular advertisement.
  advertising_data.is_extended_advertisement = false;
  advertising_data.service_data = {
      {std::string(kCopresenceServiceUuid), advertisement_header_bytes}};
  EXPECT_TRUE(ble_b.StartAdvertising(
      advertising_data,
      {.tx_power_level = kTxPowerLevel, .is_connectable = true}));

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopScanning());
  EXPECT_TRUE(ble_b.StopAdvertising());
  env_.Stop();
}

TEST_F(BleV2MediumTest, CanStartGattServer) {
  env_.Start();
  BluetoothAdapter adapter;
  BleV2Medium ble(adapter);
  std::string characteristic_uuid = "characteristic_uuid";

  std::unique_ptr<GattServer> gatt_server =
      ble.StartGattServer(/*ServerGattConnectionCallback=*/{});

  ASSERT_NE(gatt_server, nullptr);

  std::vector<GattCharacteristic::Permission> permissions = {
      GattCharacteristic::Permission::kRead};
  std::vector<GattCharacteristic::Property> properties = {
      GattCharacteristic::Property::kRead};
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<GattCharacteristic> gatt_characteristic =
      gatt_server->CreateCharacteristic(std::string(kCopresenceServiceUuid),
                                        characteristic_uuid, permissions,
                                        properties);

  ASSERT_TRUE(gatt_characteristic.has_value());

  ByteArray any_byte("any");

  EXPECT_TRUE(
      gatt_server->UpdateCharacteristic(gatt_characteristic.value(), any_byte));

  gatt_server->Stop();

  env_.Stop();
}

TEST_F(BleV2MediumTest, GattClientConnectToGattServerWorks) {
  env_.Start();
  BluetoothAdapter adapter_a;
  BluetoothAdapter adapter_b;
  BleV2Medium ble_a(adapter_a);
  BleV2Medium ble_b(adapter_b);
  std::string characteristic_uuid = "characteristic_uuid";

  // Start GattServer
  std::unique_ptr<GattServer> gatt_server =
      ble_a.StartGattServer(/*ServerGattConnectionCallback=*/{});

  ASSERT_NE(gatt_server, nullptr);

  std::vector<GattCharacteristic::Permission> permissions = {
      GattCharacteristic::Permission::kRead};
  std::vector<GattCharacteristic::Property> properties = {
      GattCharacteristic::Property::kRead};
  // Add characteristic and its value.
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<GattCharacteristic> server_characteristic =
      gatt_server->CreateCharacteristic(std::string(kCopresenceServiceUuid),
                                        characteristic_uuid, permissions,
                                        properties);
  ASSERT_TRUE(server_characteristic.has_value());
  ByteArray server_value("any");
  EXPECT_TRUE(
      gatt_server->UpdateCharacteristic(*server_characteristic, server_value));

  // Start GattClient
  auto ble_peripheral =
      std::make_unique<BlePeripheralStub>(/*mac_address=*/"ABCD");
  std::unique_ptr<GattClient> gatt_client = ble_b.ConnectToGattServer(
      BleV2Peripheral(ble_peripheral.get()), kTxPowerLevel,
      /*ClientGattConnectionCallback=*/{});

  ASSERT_NE(gatt_client, nullptr);

  // Discover service.
  EXPECT_TRUE(
      gatt_client->DiscoverService(std::string(kCopresenceServiceUuid)));

  // Discover characteristic.
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<GattCharacteristic> client_characteristic =
      gatt_client->GetCharacteristic(std::string(kCopresenceServiceUuid),
                                     characteristic_uuid);
  ASSERT_TRUE(client_characteristic.has_value());

  // Can read the characteristic value.
  EXPECT_THAT(gatt_client->ReadCharacteristic(*client_characteristic),
              Optional(server_value));

  gatt_client->Disconnect();
  gatt_server->Stop();
  env_.Stop();
}

}  // namespace
}  // namespace nearby
}  // namespace location
