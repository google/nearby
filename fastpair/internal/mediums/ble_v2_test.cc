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

#include "fastpair/internal/mediums/ble_v2.h"

#include "gtest/gtest.h"

namespace nearby {
namespace fastpair {
namespace {

TEST(BleV2Test, IsAvailable) {
  BluetoothRadio radio;
  BleV2 bleV2(radio);
  EXPECT_TRUE(bleV2.IsAvailable());
  EXPECT_TRUE(radio.Disable());
  EXPECT_FALSE(bleV2.IsAvailable());
}

TEST(BleV2Test, CanConnectToGattServer) {
  BluetoothRadio radio;
  BleV2 bleV2(radio);
  EXPECT_TRUE(bleV2.ConnectToGattServer("bleaddress", {}, nullptr));
}

TEST(BleV2Test, CannotConnectToGattServer) {
  BluetoothRadio radio;
  BleV2 bleV2(radio);
  EXPECT_TRUE(radio.Disable());
  EXPECT_FALSE(bleV2.ConnectToGattServer("bleaddress", {}, nullptr));
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
