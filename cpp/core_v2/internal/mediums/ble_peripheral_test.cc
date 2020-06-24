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

#include "core_v2/internal/mediums/ble_peripheral.h"

#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {
namespace {

constexpr absl::string_view kId{"AB12"};

TEST(BlePeripheralTest, ConstructionWorks) {
  ByteArray id{std::string(kId)};

  BlePeripheral ble_peripheral{id};

  EXPECT_TRUE(ble_peripheral.IsValid());
  EXPECT_EQ(id, ble_peripheral.GetId());
}

TEST(BlePeripheralTest, ConstructionEmptyFails) {
  BlePeripheral ble_peripheral;

  EXPECT_FALSE(ble_peripheral.IsValid());
  EXPECT_TRUE(ble_peripheral.GetId().Empty());
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
