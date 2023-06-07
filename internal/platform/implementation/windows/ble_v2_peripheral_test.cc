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

#include "internal/platform/implementation/windows/ble_v2_peripheral.h"

#include "gtest/gtest.h"

namespace nearby {
namespace windows {
namespace {

TEST(BleV2Peripheral, Constructor) {
  constexpr absl::string_view kAddress = "F1:F2:F3:F4:F5:F6";
  BleV2Peripheral ble_peripheral(kAddress);

  EXPECT_TRUE(ble_peripheral);
  EXPECT_TRUE(ble_peripheral.Ok());
  EXPECT_NE(ble_peripheral.GetUniqueId(), 0);
  EXPECT_EQ(ble_peripheral.GetAddress(), kAddress);
}

TEST(BleV2Peripheral, SetMacAddress) {
  BleV2Peripheral ble_peripheral("F1:F2:F3:F4:F5:F6");
  EXPECT_TRUE(ble_peripheral.SetAddress("00:B0:D0:63:C2:26"));
  EXPECT_FALSE(ble_peripheral.SetAddress("00:B0:D0:6T:C2:26"));
  EXPECT_FALSE(ble_peripheral.SetAddress("00:B0:D0:63:C2:2"));
  EXPECT_FALSE(ble_peripheral.SetAddress("0:B0:D0:63:C2:203"));
  EXPECT_FALSE(ble_peripheral.SetAddress("0:B0:D0:6P:C2:203"));
}

TEST(BleV2Peripheral, SetAndGetMacAddress) {
  constexpr absl::string_view kChangedAddress = "00:B0:D0:63:C2:26";
  BleV2Peripheral ble_peripheral("F1:F2:F3:F4:F5:F6");

  EXPECT_TRUE(ble_peripheral.SetAddress(kChangedAddress));
  EXPECT_EQ(ble_peripheral.GetAddress(), kChangedAddress);
}

TEST(BleV2Peripheral, SetAddressDoesNotChangeUniqueId) {
  constexpr absl::string_view kChangedAddress = "00:B0:D0:63:C2:26";
  BleV2Peripheral ble_peripheral("F1:F2:F3:F4:F5:F6");
  BleV2Peripheral::UniqueId unique_id = ble_peripheral.GetUniqueId();

  EXPECT_NE(unique_id, 0);
  EXPECT_TRUE(ble_peripheral.SetAddress(kChangedAddress));
  EXPECT_EQ(ble_peripheral.GetAddress(), kChangedAddress);
  EXPECT_EQ(ble_peripheral.GetUniqueId(), unique_id);
}

TEST(BleV2Peripheral, ConstructFromBadAddress) {
  BleV2Peripheral ble_peripheral("G1:F2:F3:F4:F5:F6");

  EXPECT_FALSE(ble_peripheral);
  EXPECT_FALSE(ble_peripheral.Ok());
  EXPECT_EQ(ble_peripheral.GetUniqueId(), 0);
  EXPECT_EQ(ble_peripheral.GetAddress(), "");
}

}  // namespace
}  // namespace windows
}  // namespace nearby
