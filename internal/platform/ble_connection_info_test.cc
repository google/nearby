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

#include "internal/platform/ble_connection_info.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/platform/connection_info.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace {

using Medium = ::location::nearby::proto::connections::Medium;
using ::testing::status::StatusIs;

constexpr absl::string_view kMacAddr = "\x4C\x8B\x1D\xCE\xBA\xD1";
constexpr absl::string_view kGattCharacteristic =
    "\x03\x0a\x13\x56\x67\x21\x12\x45";
constexpr absl::string_view kPsm = "\x45\x56";
constexpr char kAction = 0x0F;

TEST(BleConnectionInfoTest, TestGetFields) {
  BleConnectionInfo info(kMacAddr, kGattCharacteristic, kPsm, {kAction});
  EXPECT_EQ(info.GetMediumType(), Medium::BLE);
  EXPECT_EQ(info.GetGattCharacteristic(), kGattCharacteristic);
  ASSERT_EQ(info.GetActions().size(), 1);
  ASSERT_EQ(info.GetActions()[0], kAction);
  EXPECT_EQ(info.GetMacAddress(), kMacAddr);
  EXPECT_EQ(info.GetPsm(), kPsm);
}

TEST(BleConnectionInfoTest, TestFromEmptyBytes) {
  auto result = BleConnectionInfo::FromDataElementBytes("");
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(BleConnectionInfoTest, TestFromInvalidBytes) {
  auto result = BleConnectionInfo::FromDataElementBytes(kMacAddr);
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(BleConnectionInfoTest, TestFromNoAction) {
  BleConnectionInfo info(kMacAddr, kGattCharacteristic, kPsm, {kAction});
  std::string serialized = info.ToDataElementBytes();
  serialized[1] -= 1;
  auto result = BleConnectionInfo::FromDataElementBytes(
      serialized.substr(0, serialized.length() - 1));
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(BleConnectionInfoTest, TestToFromBytes) {
  BleConnectionInfo info(kMacAddr, kGattCharacteristic, kPsm, {kAction});
  std::string serialized = info.ToDataElementBytes();
  auto result = BleConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_OK(result);
  auto info2 = result.value();
  EXPECT_EQ(info2.GetMacAddress(), info.GetMacAddress());
  EXPECT_EQ(info2.GetGattCharacteristic(), info.GetGattCharacteristic());
  EXPECT_EQ(info2.GetActions(), info.GetActions());
  EXPECT_EQ(info2.GetPsm(), info.GetPsm());
}

TEST(BleConnectionInfoTest, TestToFromBytesNoGattCharacteristic) {
  BleConnectionInfo info(kMacAddr, "", kPsm, {kAction});
  std::string serialized = info.ToDataElementBytes();
  auto result = BleConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_OK(result);
  auto info2 = result.value();
  EXPECT_EQ(info2.GetMacAddress(), info.GetMacAddress());
  EXPECT_EQ(info2.GetGattCharacteristic(), "");
  EXPECT_EQ(info2.GetActions(), info.GetActions());
  EXPECT_EQ(info2.GetPsm(), info.GetPsm());
}

TEST(BleConnectionInfoTest, TestToFromBytesNoMac) {
  BleConnectionInfo info("", kGattCharacteristic, kPsm, {kAction});
  std::string serialized = info.ToDataElementBytes();
  auto result = BleConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_OK(result);
  auto info2 = result.value();
  EXPECT_EQ(info2.GetMacAddress(), "");
  EXPECT_EQ(info2.GetGattCharacteristic(), info.GetGattCharacteristic());
  EXPECT_EQ(info2.GetActions(), info.GetActions());
  EXPECT_EQ(info2.GetPsm(), info.GetPsm());
}

TEST(BleConnectionInfoTest, TestToFromBytesLongMac) {
  BleConnectionInfo info(absl::StrCat(kMacAddr, kMacAddr), kGattCharacteristic,
                         kPsm, {kAction});
  std::string serialized = info.ToDataElementBytes();
  auto result = BleConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_OK(result);
  auto info2 = result.value();
  EXPECT_EQ(info2.GetMacAddress(), "");
  EXPECT_EQ(info2.GetGattCharacteristic(), info.GetGattCharacteristic());
  EXPECT_EQ(info2.GetActions(), info.GetActions());
  EXPECT_EQ(info2.GetPsm(), info.GetPsm());
}

TEST(BleConnectionInfoTest, TestToFromBytesNoPsm) {
  BleConnectionInfo info(kMacAddr, kGattCharacteristic, "", {kAction});
  std::string serialized = info.ToDataElementBytes();
  auto result = BleConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_OK(result);
  auto info2 = result.value();
  EXPECT_EQ(info2.GetMacAddress(), info.GetMacAddress());
  EXPECT_EQ(info2.GetGattCharacteristic(), info.GetGattCharacteristic());
  EXPECT_EQ(info2.GetActions(), info.GetActions());
  EXPECT_EQ(info2.GetPsm(), "");
}

TEST(BleConnectionInfoTest, TestToFromBytesLongPsm) {
  BleConnectionInfo info(kMacAddr, kGattCharacteristic,
                         absl::StrCat(kPsm, kPsm), {kAction});
  std::string serialized = info.ToDataElementBytes();
  auto result = BleConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_OK(result);
  auto info2 = result.value();
  EXPECT_EQ(info2.GetMacAddress(), info.GetMacAddress());
  EXPECT_EQ(info2.GetGattCharacteristic(), info.GetGattCharacteristic());
  EXPECT_EQ(info2.GetActions(), info.GetActions());
  EXPECT_EQ(info2.GetPsm(), "");
}

TEST(BleConnectionInfoTest, TestFromEmpty) {
  auto result = BleConnectionInfo::FromDataElementBytes("");
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(BleConnectionInfoTest, TestFromBadElementType) {
  BleConnectionInfo info(kMacAddr, kGattCharacteristic, kPsm, {kAction});
  std::string serialized = info.ToDataElementBytes();
  serialized[0] = 0x56;
  auto result = BleConnectionInfo::FromDataElementBytes(serialized);
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(BleConnectionInfoTest, TestCopy) {
  BleConnectionInfo info(kMacAddr, kGattCharacteristic, kPsm, {kAction});
  BleConnectionInfo copy(info);
  EXPECT_EQ(info, copy);
}

TEST(BleConnectionInfoTest, TestEquals) {
  BleConnectionInfo info(kMacAddr, kGattCharacteristic, kPsm, {kAction});
  BleConnectionInfo info2(kMacAddr, kGattCharacteristic, kPsm, {kAction});
  EXPECT_EQ(info, info2);
}

}  // namespace
}  // namespace nearby
