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

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/medium_environment.h"

namespace location {
namespace nearby {
namespace {

using ::location::nearby::api::ble_v2::BleAdvertisementData;
using ::location::nearby::api::ble_v2::GattCharacteristic;
using ::location::nearby::api::ble_v2::PowerMode;

constexpr absl::string_view kAdvertisementString{"\x0a\x0b\x0c\x0d"};
constexpr absl::string_view kCoprsenseServiceUuid{"F3FE"};
constexpr absl::string_view kFastAdvertisementServiceUuid{"FAST"};
constexpr PowerMode kPowerMode(PowerMode::kHigh);

class BleV2MediumTest : public testing::Test {
 protected:
  BleV2MediumTest() { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(BleV2MediumTest, ConstructorDestructorWorks) {
  env_.Start();
  BluetoothAdapter adapter_a_;
  BluetoothAdapter adapter_b_;
  BleV2Medium ble_a{adapter_a_};
  BleV2Medium ble_b{adapter_b_};

  // Make sure we can create functional mediums.
  ASSERT_TRUE(ble_a.IsValid());
  ASSERT_TRUE(ble_b.IsValid());

  // Make sure we can create 2 distinct mediums.
  EXPECT_NE(ble_a.GetImpl(), ble_b.GetImpl());
  env_.Stop();
}

TEST_F(BleV2MediumTest, CanStartFastAdvertising) {
  env_.Start();
  BluetoothAdapter adapter_;
  BleV2Medium ble{adapter_};
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  std::string fast_advertisement_service_uuid(kFastAdvertisementServiceUuid);

  BleAdvertisementData advertising_data;
  advertising_data.is_connectable = true;
  advertising_data.tx_power_level =
      BleAdvertisementData::kUnspecifiedTxPowerLevel;
  advertising_data.service_uuids.insert(fast_advertisement_service_uuid);

  // Assemble fast advertising and scan response data.
  BleAdvertisementData scan_response_data;
  scan_response_data.is_connectable = true;
  scan_response_data.tx_power_level =
      BleAdvertisementData::kUnspecifiedTxPowerLevel;
  scan_response_data.service_data.insert(
      {fast_advertisement_service_uuid, advertisement_bytes});

  EXPECT_TRUE(
      ble.StartAdvertising(advertising_data, scan_response_data, kPowerMode));

  EXPECT_TRUE(ble.StopAdvertising());
  env_.Stop();
}

TEST_F(BleV2MediumTest, CanStartAdvertising) {
  env_.Start();
  BluetoothAdapter adapter_;
  BleV2Medium ble{adapter_};
  ByteArray advertisement_header_bytes{std::string(kAdvertisementString)};
  std::string fast_advertisement_service_uuid(kFastAdvertisementServiceUuid);

  // Assemble regular advertising and scan response data.
  BleAdvertisementData advertising_data;
  advertising_data.is_connectable = true;
  advertising_data.tx_power_level =
      BleAdvertisementData::kUnspecifiedTxPowerLevel;

  BleAdvertisementData scan_response_data;
  scan_response_data.is_connectable = true;
  scan_response_data.tx_power_level =
      BleAdvertisementData::kUnspecifiedTxPowerLevel;
  scan_response_data.service_uuids.insert(fast_advertisement_service_uuid);
  scan_response_data.service_data.insert(
      {fast_advertisement_service_uuid, advertisement_header_bytes});

  EXPECT_TRUE(
      ble.StartAdvertising(advertising_data, scan_response_data, kPowerMode));

  EXPECT_TRUE(ble.StopAdvertising());
  env_.Stop();
}

TEST_F(BleV2MediumTest, CanStartGattServer) {
  env_.Start();
  BluetoothAdapter adapter_;
  BleV2Medium ble{adapter_};
  std::string characteristic_uuid = "characteristic_uuid";

  std::unique_ptr<GattServer> gatt_server = ble.StartGattServer({});

  ASSERT_NE(gatt_server, nullptr);

  std::vector<GattCharacteristic::Permission> permissions{
      GattCharacteristic::Permission::kRead};
  std::vector<GattCharacteristic::Property> properties{
      GattCharacteristic::Property::kRead};
  absl::optional<GattCharacteristic> gatt_characteristic =
      gatt_server->CreateCharacteristic(std::string(kCoprsenseServiceUuid),
                                        characteristic_uuid, permissions,
                                        properties);

  ASSERT_TRUE(gatt_characteristic.has_value());

  ByteArray any_byte("any");

  EXPECT_TRUE(
      gatt_server->UpdateCharacteristic(gatt_characteristic.value(), any_byte));

  gatt_server->Stop();

  env_.Stop();
}

TEST_F(BleV2MediumTest, CanStartScanning) {
  env_.Start();
  BluetoothAdapter adapter_;
  BleV2Medium ble{adapter_};
  std::vector<std::string> service_uuids{std::string(kCoprsenseServiceUuid)};

  EXPECT_TRUE(ble.StartScanning(
      service_uuids, kPowerMode,
      {
          .advertisement_found_cb =
              [](BleV2Peripheral& peripheral,
                 const BleAdvertisementData& advertisement_data) {
                // nothing to do for now
              },
      }));
  EXPECT_TRUE(ble.StopScanning());
  env_.Stop();
}

}  // namespace
}  // namespace nearby
}  // namespace location
