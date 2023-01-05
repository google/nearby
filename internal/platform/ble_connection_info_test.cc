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

#include "internal/platform/ble_connection_info.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace {

constexpr absl::string_view kMacAddr = "\x4C\x8B\x1D\xCE\xBA\xD1";

TEST(BleConnectionInfoTest, TestMediumType) {
  BleConnectionInfo info(kMacAddr);
  EXPECT_EQ(info.GetMediumType(), BleConnectionInfo::MediumType::kBle);
}

TEST(BleConnectionInfoTest, TestToBytes) {
  ByteArray mac_addr_bytes = ByteArray(std::string(kMacAddr));
  BleConnectionInfo info(kMacAddr);
  EXPECT_EQ(info.ToBytes(), mac_addr_bytes);
}

TEST(BleConnectionInfoTest, TestFromBytes) {
  ByteArray mac_addr_bytes = ByteArray(std::string(kMacAddr));
  BleConnectionInfo info = BleConnectionInfo::FromBytes(mac_addr_bytes);
  EXPECT_EQ(info.GetMacAddress(), mac_addr_bytes);
}

TEST(BleConnectionInfoTest, TestGetMacAddress) {
  ByteArray mac_addr_bytes = ByteArray(std::string(kMacAddr));
  BleConnectionInfo info(kMacAddr);
  EXPECT_EQ(info.GetMacAddress(), mac_addr_bytes);
}

TEST(BleConnectionInfoTest, TestGetLongMacAddr) {
  BleConnectionInfo info(absl::StrCat(kMacAddr, "\x56\x70\x89"));
  EXPECT_EQ(info.GetMacAddress().AsStringView(), kDefunctMacAddr);
}

TEST(BleConnectionInfoTest, TestGetShortMacAddr) {
  BleConnectionInfo info("\x56\x70\x89");
  EXPECT_EQ(info.GetMacAddress().AsStringView(), kDefunctMacAddr);
}

TEST(BleConnectionInfoTest, TestToFromBytes) {
  BleConnectionInfo info(kMacAddr);
  ByteArray serialized = info.ToBytes();
  BleConnectionInfo result = BleConnectionInfo::FromBytes(serialized);
  EXPECT_EQ(result, info);
}

TEST(BleConnectionInfoTest, TestCopy) {
  BleConnectionInfo info(kMacAddr);
  BleConnectionInfo copy(info);
  EXPECT_EQ(info, copy);
}

TEST(BleConnectionInfoTest, TestEquals) {
  BleConnectionInfo info(kMacAddr);
  BleConnectionInfo info2(kMacAddr);
  EXPECT_EQ(info, info2);
}

}  // namespace
}  // namespace nearby
