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

#include "fastpair/internal/mediums/mediums.h"

#include "gtest/gtest.h"

namespace nearby {
namespace fastpair {
namespace {

TEST(MediumTest, ConstructorWorks) {
  Mediums medium;
  EXPECT_TRUE(medium.GetBluetoothRadio().IsAdapterValid());
  EXPECT_FALSE(medium.GetBle().IsScanning());
  EXPECT_TRUE(
      medium.GetBleV2().ConnectToGattServer("ble_address", {}, nullptr));
  EXPECT_TRUE(medium.GetBluetoothClassic().IsAvailable());
}
}  // namespace
}  // namespace fastpair
}  // namespace nearby
