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
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "internal/platform/ble_connection_info.h"
#include "internal/platform/bluetooth_connection_info.h"
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
  BluetoothConnectionInfo info(kMacAddr, kBluetoothUuid, GetDefaultActions());
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

TEST(ConnectionInfoTest, TestMonostate) {
  WifiLanConnectionInfo wifi_info(kIpv4Addr, kPort, kBssid,
                                  GetDefaultActions());
  BluetoothConnectionInfo bt_info(kMacAddr, kBluetoothUuid,
                                  GetDefaultActions());
  BleConnectionInfo ble_info(kMacAddr, kGattCharacteristic, kPsm,
                             GetDefaultActions());
  std::vector<ConnectionInfo*> infos = {&bt_info, &ble_info, &wifi_info};
  for (auto info : infos) {
    auto serialized = info->ToDataElementBytes();
    auto connection_info =
        ConnectionInfo::FromDataElementBytes(serialized.substr(0, 10));
    EXPECT_TRUE(absl::holds_alternative<absl::monostate>(connection_info));
  }
}

TEST(ConnectionInfoTest, TestCannotRestoreAsOtherInfos) {
  WifiLanConnectionInfo wifi_info(kIpv4Addr, kPort, kBssid,
                                  GetDefaultActions());
  BluetoothConnectionInfo bt_info(kMacAddr, kBluetoothUuid,
                                  GetDefaultActions());
  BleConnectionInfo ble_info(kMacAddr, kGattCharacteristic, kPsm,
                             GetDefaultActions());
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
}

}  // namespace
}  // namespace nearby
