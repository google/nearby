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

#include "internal/platform/bluetooth_connection_info.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace nearby {
namespace {

constexpr absl::string_view kMacAddr = "\x4C\x8B\x1D\xCE\xBA\xD1";
constexpr absl::string_view kServiceId{"test"};

TEST(BluetoothConnectionInfoTest, TestMediumType) {
  std::string macAddr(kMacAddr);
  BluetoothConnectionInfo info(ByteArray(macAddr), kServiceId);
  EXPECT_EQ(info.GetMediumType(),
            BluetoothConnectionInfo::MediumType::kBluetooth);
}

TEST(BluetoothConnectionInfoTest, TestToBytes) {
  std::string macAddr(kMacAddr);
  BluetoothConnectionInfo info(ByteArray(macAddr), kServiceId);
  ByteArray serialized_expected =
      ByteArray(absl::StrCat(kMacAddr, kServiceId));
  EXPECT_EQ(info.ToBytes(), serialized_expected);
}

TEST(BluetoothConnectionInfoTest, TestFromBytes) {
  std::string macAddr(kMacAddr);
  ByteArray serialized =
      ByteArray(absl::StrCat(macAddr, kServiceId));
  BluetoothConnectionInfo info = BluetoothConnectionInfo::FromBytes(serialized);
  EXPECT_EQ(info.GetMacAddress(), ByteArray(macAddr));
  EXPECT_EQ(info.GetServiceId(), kServiceId);
}

TEST(BluetoothConnectionInfoTest, TestGetMacAddress) {
  std::string macAddr(kMacAddr);
  BluetoothConnectionInfo info(ByteArray(macAddr), kServiceId);
  EXPECT_EQ(info.GetMacAddress(), ByteArray(macAddr));
}

TEST(BluetoothConnectionInfoTest, TestGetLongMacAddr) {
  std::string macAddr(kMacAddr);
  BluetoothConnectionInfo info(ByteArray(macAddr + "\x56\x70\x89"), kServiceId);
  EXPECT_EQ(info.GetMacAddress().AsStringView(), kMacAddr);
}

TEST(BluetoothConnectionInfoTest, TestGetServiceId) {
  std::string macAddr(kMacAddr);
  BluetoothConnectionInfo info(ByteArray(macAddr), kServiceId);
  EXPECT_EQ(info.GetServiceId(), kServiceId);
}

TEST(BluetoothConnectionInfoTest, TestToFromBytes) {
  std::string macAddr(kMacAddr);
  BluetoothConnectionInfo info(ByteArray(macAddr), kServiceId);
  ByteArray serialized = info.ToBytes();
  BluetoothConnectionInfo result =
      BluetoothConnectionInfo::FromBytes(serialized);
  EXPECT_EQ(result, info);
}

TEST(BluetoothConnectionInfoTest, TestCopy) {
  std::string macAddr(kMacAddr);
  BluetoothConnectionInfo info(ByteArray(macAddr), kServiceId);
  BluetoothConnectionInfo copy(info);
  EXPECT_EQ(info, copy);
}

TEST(BluetoothConnectionInfoTest, TestEquals) {
  std::string macAddr(kMacAddr);
  BluetoothConnectionInfo info(ByteArray(macAddr), kServiceId);
  BluetoothConnectionInfo info2(ByteArray(macAddr), kServiceId);
  EXPECT_EQ(info, info2);
}

}  // namespace
}  // namespace nearby
