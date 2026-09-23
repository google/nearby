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

#include "internal/platform/connection_info.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "internal/platform/ble_connection_info.h"
#include "internal/platform/bluetooth_connection_info.h"
#include "internal/platform/mac_address.h"
#include "internal/platform/wifi_aware_connection_info.h"
#include "internal/platform/wifi_lan_connection_info.h"

namespace nearby {
namespace {

// Common
constexpr uint8_t kFirstAction = 0x0F;
constexpr uint8_t kSecondAction = 0x04;
// BLE
constexpr absl::string_view kMacAddr = "\x4C\x8B\x1D\xCE\xBA\xD1";
constexpr absl::string_view kGattCharacteristic =
    "\x03\x0a\x13\x56\x67\x21\x12\x45";
constexpr absl::string_view kPsm = "\x45\x56";
// Bluetooth
constexpr absl::string_view kBluetoothUuid{"test"};
// WLAN
constexpr absl::string_view kIpv4Addr = "\x4C\x8B\x1D\xCE";
constexpr absl::string_view kPort = "\x12\x34";
constexpr absl::string_view kBssid = "\x0A\x1B\x2C\x34\x58\x7E";
// Wi-Fi Aware
constexpr absl::string_view kServiceId = "test_service_id";

std::vector<uint8_t> GetDefaultActions() {
  return {kFirstAction, kSecondAction};
}

TEST(ConnectionInfoTest, TestRestoreBle) {
  BleConnectionInfo info(kMacAddr, kGattCharacteristic, kPsm,
                         GetDefaultActions());
  auto serialized = info.ToDataElementBytes();
  auto connection_info = ConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_TRUE(absl::holds_alternative<BleConnectionInfo>(connection_info));
  auto ble_connection_info = absl::get<BleConnectionInfo>(connection_info);
  EXPECT_EQ(ble_connection_info, info);
}

TEST(ConnectionInfoTest, TestRestoreBluetooth) {
  MacAddress mac_address;
  MacAddress::FromBytes(
      absl::MakeSpan(reinterpret_cast<const uint8_t*>(kMacAddr.data()),
                     kMacAddr.size()),
      mac_address);
  BluetoothConnectionInfo info(mac_address, kBluetoothUuid,
                               GetDefaultActions());
  auto serialized = info.ToDataElementBytes();
  auto connection_info = ConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_TRUE(
      absl::holds_alternative<BluetoothConnectionInfo>(connection_info));
  auto bt_connection_info = absl::get<BluetoothConnectionInfo>(connection_info);
  EXPECT_EQ(bt_connection_info, info);
}

TEST(ConnectionInfoTest, TestRestoreMdns) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, kBssid, GetDefaultActions());
  auto serialized = info.ToDataElementBytes();
  auto connection_info = ConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_TRUE(absl::holds_alternative<WifiLanConnectionInfo>(connection_info));
  auto wlan_connection_info = absl::get<WifiLanConnectionInfo>(connection_info);
  EXPECT_EQ(wlan_connection_info, info);
}

TEST(ConnectionInfoTest, TestRestoreWifiAware) {
  WifiAwareConnectionInfo info(kServiceId, GetDefaultActions());
  auto serialized = info.ToDataElementBytes();
  auto connection_info = ConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_TRUE(
      absl::holds_alternative<WifiAwareConnectionInfo>(connection_info));
  auto wifi_aware_connection_info =
      absl::get<WifiAwareConnectionInfo>(connection_info);
  EXPECT_EQ(wifi_aware_connection_info, info);
}

TEST(ConnectionInfoTest, TestRestoreWifiAwareShortBytes) {
  // Empty
  EXPECT_THAT(WifiAwareConnectionInfo::FromDataElementBytes(""),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
  // 1 byte
  EXPECT_THAT(WifiAwareConnectionInfo::FromDataElementBytes("\x14"),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
  // 2 bytes with 0 length
  std::string two_bytes = {'\x14', '\x00'};
  EXPECT_THAT(WifiAwareConnectionInfo::FromDataElementBytes(two_bytes),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
  // 3 bytes
  std::string three_bytes = {'\x14', '\x01', '\x04'};
  EXPECT_THAT(WifiAwareConnectionInfo::FromDataElementBytes(three_bytes),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
  // 4 bytes
  std::string four_bytes = {'\x14', '\x02', '\x04', '\x10'};
  EXPECT_THAT(WifiAwareConnectionInfo::FromDataElementBytes(four_bytes),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConnectionInfoTest, TestMonostate) {
  MacAddress mac_address;
  MacAddress::FromBytes(
      absl::MakeSpan(reinterpret_cast<const uint8_t*>(kMacAddr.data()),
                     kMacAddr.size()),
      mac_address);
  WifiLanConnectionInfo wifi_info(kIpv4Addr, kPort, kBssid,
                                  GetDefaultActions());
  BluetoothConnectionInfo bt_info(mac_address, kBluetoothUuid,
                                  GetDefaultActions());
  BleConnectionInfo ble_info(kMacAddr, kGattCharacteristic, kPsm,
                             GetDefaultActions());
  WifiAwareConnectionInfo wifi_aware_info(kServiceId, GetDefaultActions());
  std::vector<ConnectionInfo*> infos = {&bt_info, &ble_info, &wifi_info,
                                        &wifi_aware_info};
  for (auto info : infos) {
    auto serialized = info->ToDataElementBytes();
    auto connection_info =
        ConnectionInfo::FromDataElementBytes(serialized.substr(0, 10));
    EXPECT_TRUE(absl::holds_alternative<absl::monostate>(connection_info));
  }
}

TEST(ConnectionInfoTest, TestFromDataElementBytesInvalidOrShort) {
  // Empty
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      ConnectionInfo::FromDataElementBytes("")));
  // 1 byte
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      ConnectionInfo::FromDataElementBytes("\x14")));
  // 2 bytes
  std::string two_bytes = {'\x14', '\x00'};
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      ConnectionInfo::FromDataElementBytes(two_bytes)));
  // Wrong data element field type
  std::string bad_type = {'\x15', '\x05', '\x04'};
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      ConnectionInfo::FromDataElementBytes(bad_type)));
}

TEST(ConnectionInfoTest, TestCannotRestoreAsOtherInfos) {
  MacAddress mac_address;
  MacAddress::FromBytes(
      absl::MakeSpan(reinterpret_cast<const uint8_t*>(kMacAddr.data()),
                     kMacAddr.size()),
      mac_address);
  WifiLanConnectionInfo wifi_info(kIpv4Addr, kPort, kBssid,
                                  GetDefaultActions());
  BluetoothConnectionInfo bt_info(mac_address, kBluetoothUuid,
                                  GetDefaultActions());
  BleConnectionInfo ble_info(kMacAddr, kGattCharacteristic, kPsm,
                             GetDefaultActions());
  WifiAwareConnectionInfo wifi_aware_info(kServiceId, GetDefaultActions());
  EXPECT_THAT(
      BleConnectionInfo::FromDataElementBytes(wifi_info.ToDataElementBytes()),
      testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(
      BleConnectionInfo::FromDataElementBytes(bt_info.ToDataElementBytes()),
      testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(BluetoothConnectionInfo::FromDataElementBytes(
                  ble_info.ToDataElementBytes()),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(BluetoothConnectionInfo::FromDataElementBytes(
                  wifi_info.ToDataElementBytes()),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(WifiLanConnectionInfo::FromDataElementBytes(
                  ble_info.ToDataElementBytes()),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(
      WifiLanConnectionInfo::FromDataElementBytes(bt_info.ToDataElementBytes()),
      testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(WifiAwareConnectionInfo::FromDataElementBytes(
                  ble_info.ToDataElementBytes()),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(BleConnectionInfo::FromDataElementBytes(
                  wifi_aware_info.ToDataElementBytes()),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace nearby
