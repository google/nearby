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
#include <string>

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

TEST(BluetoothUtil, ConvertBluetoothAddressUIntToString) {
  const uint64_t input = 0x00001A2B3C4D5E6F;
  std::string expected_output = "1A:2B:3C:4D:5E:6F";
  std::string output = ConvertBluetoothAddressUIntToString(input);
  EXPECT_EQ(output, expected_output);
}

TEST(BluetoothUtil, CanonicalizeBluetoothAddress) {
  std::array<uint8_t, 6> address{{26, 43, 60, 77, 94, 111}};
  EXPECT_EQ(CanonicalizeBluetoothAddress(address), "1A:2B:3C:4D:5E:6F");
  EXPECT_EQ(CanonicalizeBluetoothAddress("1A-2B-3C-4D-5E-6F"),
            "1A:2B:3C:4D:5E:6F");
  EXPECT_EQ(CanonicalizeBluetoothAddress(0x00001A2B3C4D5E6F),
            "1A:2B:3C:4D:5E:6F");

  // Canonicalizes invalid address
  EXPECT_EQ(CanonicalizeBluetoothAddress("1A-2B-3C-4D-5E-6F-89"), "");
  EXPECT_EQ(CanonicalizeBluetoothAddress("nearby"), "");
  EXPECT_EQ(CanonicalizeBluetoothAddress("MA-2M-3C-4D-5E-6F"), "");
  EXPECT_EQ(CanonicalizeBluetoothAddress(0x001A2B3C4D5E6F89), "");
}

}  // namespace
}  // namespace device
}  // namespace nearby
