// Copyright 2025 Google LLC
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

#include "internal/platform/mac_address.h"
#include <cstdint>

#include "gtest/gtest.h"
#include "absl/hash/hash_testing.h"

namespace nearby {
namespace {

TEST(MacAddressTest, FromStringSuccess) {
  MacAddress mac_address;
  EXPECT_TRUE(MacAddress::FromString("00:B0:D0:63:C2:26", mac_address));
  EXPECT_EQ(mac_address.address(), 0x00B0D063C226);
  EXPECT_EQ(mac_address.ToString(), "00:B0:D0:63:C2:26");
}

TEST(MacAddressTest, FromStringMissingSeparator) {
  MacAddress mac_address;
  EXPECT_FALSE(MacAddress::FromString("00:B0:D0,63:C2:26", mac_address));
  EXPECT_EQ(mac_address.address(), 0);
}

TEST(MacAddressTest, FromStringInvalidHex) {
  MacAddress mac_address;
  EXPECT_FALSE(MacAddress::FromString("01:BG:D0,63:C2:26", mac_address));
  EXPECT_EQ(mac_address.address(), 0);
}

TEST(MacAddressTest, FromStringInvalidLength) {
  MacAddress mac_address;
  EXPECT_FALSE(MacAddress::FromString("01:BG:D0,63:C2:1", mac_address));
  EXPECT_EQ(mac_address.address(), 0);
}

TEST(MacAddressTest, FromUint64Success) {
  MacAddress mac_address;
  EXPECT_TRUE(MacAddress::FromUint64(0x00B0D063C226, mac_address));
  EXPECT_EQ(mac_address.address(), 0x00B0D063C226);
}

TEST(MacAddressTest, FromUint64InvalidAddresss) {
  MacAddress mac_address;
  EXPECT_FALSE(MacAddress::FromUint64(0x0100B0D063C226, mac_address));
  EXPECT_EQ(mac_address.address(), 0);
}

TEST(MacAddressTest, IsSetTrue) {
  MacAddress mac_address;
  EXPECT_TRUE(MacAddress::FromUint64(0x00B0D063C226, mac_address));
  EXPECT_TRUE(mac_address.IsSet());
}

TEST(MacAddressTest, IsSetFalse) {
  MacAddress mac_address;
  EXPECT_FALSE(mac_address.IsSet());
}

TEST(MacAddressTest, Hash) {
  MacAddress addr1;
  EXPECT_TRUE(MacAddress::FromUint64(0x00B0D063C226, addr1));
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      MacAddress(),
      addr1,
  }));
}

TEST(MacAddressTest, ToBytesSuccess) {
  MacAddress mac_address;
  EXPECT_TRUE(MacAddress::FromUint64(0x00B0D063C226, mac_address));
  uint8_t bytes[6];
  EXPECT_TRUE(mac_address.ToBytes(bytes));
  EXPECT_EQ(bytes[0], 0x00);
  EXPECT_EQ(bytes[1], 0xB0);
  EXPECT_EQ(bytes[2], 0xD0);
  EXPECT_EQ(bytes[3], 0x63);
  EXPECT_EQ(bytes[4], 0xC2);
  EXPECT_EQ(bytes[5], 0x26);
}

TEST(MacAddressTest, ToBytesInvalidLength) {
  MacAddress mac_address;
  EXPECT_TRUE(MacAddress::FromUint64(0x00B0D063C226, mac_address));
  uint8_t bytes[5];
  EXPECT_FALSE(mac_address.ToBytes(bytes));
}

TEST(MacAddressTest, FromBytesSuccess) {
  uint8_t bytes[6] = {0x00, 0xB0, 0xD0, 0x63, 0xC2, 0x26};
  MacAddress mac_address;
  EXPECT_TRUE(MacAddress::FromBytes(bytes, mac_address));
  EXPECT_EQ(mac_address.address(), 0x00B0D063C226);
}

TEST(MacAddressTest, FromBytesInvalidLength) {
  uint8_t bytes[5] = {0x00, 0xB0, 0xD0, 0x63, 0xC2};
  MacAddress mac_address;
  EXPECT_FALSE(MacAddress::FromBytes(bytes, mac_address));
}

}  // namespace
}  // namespace nearby
