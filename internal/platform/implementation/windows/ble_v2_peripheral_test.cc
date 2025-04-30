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
#include "absl/strings/string_view.h"
#include "internal/platform/mac_address.h"

namespace nearby {
namespace windows {
namespace {

TEST(BleV2Peripheral, Constructor) {
  constexpr absl::string_view kAddress = "F1:F2:F3:F4:F5:F6";
  MacAddress address;
  ASSERT_TRUE(MacAddress::FromString(kAddress, address));
  BleV2Peripheral ble_peripheral(address);

  EXPECT_TRUE(ble_peripheral);
  EXPECT_TRUE(ble_peripheral.Ok());
  EXPECT_EQ(ble_peripheral.GetUniqueId(), 0xf1f2f3f4f5f6);
  EXPECT_EQ(ble_peripheral.GetAddress(), kAddress);
}

TEST(BleV2Peripheral, ConstructFromBadAddress) {
  MacAddress address;
  BleV2Peripheral ble_peripheral(address);

  EXPECT_FALSE(ble_peripheral);
  EXPECT_FALSE(ble_peripheral.Ok());
  EXPECT_EQ(ble_peripheral.GetUniqueId(), 0);
  EXPECT_EQ(ble_peripheral.GetAddress(), "");
}

}  // namespace
}  // namespace windows
}  // namespace nearby
