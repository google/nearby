// Copyright 2021 Google LLC
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

#include "internal/base/bluetooth_address.h"

#include <array>

#include "gtest/gtest.h"
#include "absl/types/span.h"

namespace nearby {
namespace device {
namespace {

TEST(BluetoothUtil, ParseBluetoothAddress) {
  std::array<uint8_t, 6> output;
  std::array<uint8_t, 6> expected_output{{26, 43, 60, 77, 94, 111}};
  EXPECT_TRUE(ParseBluetoothAddress(
      "1A:2B:3C:4D:5E:6F", absl::MakeSpan(output.data(), output.size())));
  EXPECT_EQ(output, expected_output);
}

}  // namespace
}  // namespace device
}  // namespace nearby
