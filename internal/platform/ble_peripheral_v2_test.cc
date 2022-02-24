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

#include "internal/platform/ble_peripheral_v2.h"

#include "gtest/gtest.h"
#include "internal/platform/implementation/ble_v2.h"

namespace location {
namespace nearby {
namespace {

constexpr absl::string_view kPeripheralName{"NAME"};
constexpr absl::string_view kAdvertisementBytes{"AB12"};

class BleV2PeripheralStub : public api::ble_v2::BlePeripheral {
 public:
  std::string GetId() const override { return id_; }
  void SetId(absl::string_view id) { id_ = id; }

 private:
  std::string id_;
};

TEST(BleV2PeripheralTest, ConstructorDestructorWorks) {
  BleV2PeripheralStub peripheral_impl_stub;
  peripheral_impl_stub.SetId(kPeripheralName);

  BleV2Peripheral peripheral(&peripheral_impl_stub);

  EXPECT_TRUE(peripheral.IsValid());
  EXPECT_EQ(peripheral.GetId(), kPeripheralName);
}

TEST(BleV2PeripheralTest, CanSetAdvertisementBytes) {
  BleV2PeripheralStub peripheral_impl_stub;
  peripheral_impl_stub.SetId(kPeripheralName);
  ByteArray advertisement_bytes{std::string(kAdvertisementBytes)};

  BleV2Peripheral peripheral(&peripheral_impl_stub);
  peripheral.SetName(advertisement_bytes);

  EXPECT_EQ(peripheral.GetName(), advertisement_bytes);
  EXPECT_EQ(peripheral.GetId(), kPeripheralName);
}

TEST(BleV2PeripheralTest, CanMoveBlePeripheral) {
  BleV2PeripheralStub peripheral_impl_stub;
  peripheral_impl_stub.SetId(kPeripheralName);
  ByteArray advertisement_bytes{std::string(kAdvertisementBytes)};

  BleV2Peripheral peripheral(&peripheral_impl_stub);
  peripheral.SetName(advertisement_bytes);

  EXPECT_EQ(peripheral.GetName(), advertisement_bytes);
  EXPECT_EQ(peripheral.GetId(), kPeripheralName);

  BleV2Peripheral other_peripheral = std::move(peripheral);

  EXPECT_FALSE(peripheral.IsValid());
  EXPECT_NE(peripheral.GetName(), advertisement_bytes);
  EXPECT_TRUE(other_peripheral.IsValid());
  EXPECT_EQ(other_peripheral.GetName(), advertisement_bytes);
  EXPECT_EQ(other_peripheral.GetId(), kPeripheralName);
}

}  // namespace
}  // namespace nearby
}  // namespace location
