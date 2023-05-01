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

#include "internal/platform/bluetooth_connection_info.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace {

using Medium = ::location::nearby::proto::connections::Medium;
using ::testing::status::StatusIs;

constexpr absl::string_view kMacAddr = "\x4C\x8B\x1D\xCE\xBA\xD1";
constexpr absl::string_view kBluetoothUuid{"test"};
constexpr char kAction = 0x0F;

TEST(BluetoothConnectionInfoTest, TestGetFields) {
  BluetoothConnectionInfo info(kMacAddr, kBluetoothUuid, {kAction});
  EXPECT_EQ(info.GetMediumType(), Medium::BLUETOOTH);
  EXPECT_EQ(info.GetMacAddress(), kMacAddr);
  ASSERT_EQ(info.GetActions().size(), 1);
  EXPECT_EQ(info.GetActions()[0], kAction);
  EXPECT_EQ(info.GetBluetoothUuid(), kBluetoothUuid);
}

TEST(BluetoothConnectionInfoTest, TestGetLongMacAddr) {
  BluetoothConnectionInfo info(absl::StrCat(kMacAddr, "\x56\x70\x89"),
                               kBluetoothUuid, {kAction});
  EXPECT_NE(info.GetMacAddress(), kMacAddr);
}

TEST(BluetoothConnectionInfoTest, TestToFromBytes) {
  BluetoothConnectionInfo info(kMacAddr, kBluetoothUuid, {kAction});
  std::string serialized = info.ToDataElementBytes();
  auto result = BluetoothConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_OK(result);
  EXPECT_EQ(result.value(), info);
}

TEST(BluetoothConnectionInfoTest, TestToFromNoMacAddress) {
  BluetoothConnectionInfo info("", kBluetoothUuid, {kAction});
  std::string serialized = info.ToDataElementBytes();
  auto result = BluetoothConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_OK(result);
  info = result.value();
  EXPECT_EQ(info.GetMacAddress(), "");
  EXPECT_EQ(info.GetBluetoothUuid(), kBluetoothUuid);
  ASSERT_EQ(info.GetActions().size(), 1);
  EXPECT_EQ(info.GetActions()[0], kAction);
}

TEST(BluetoothConnectionInfoTest, TestToFromWrongLength) {
  BluetoothConnectionInfo info(kMacAddr, kBluetoothUuid, {kAction});
  std::string serialized = info.ToDataElementBytes();
  ++serialized[1];
  auto result = BluetoothConnectionInfo::FromDataElementBytes(serialized);
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(BluetoothConnectionInfoTest, TestToFromNoBluetoothUuid) {
  BluetoothConnectionInfo info(kMacAddr, "", {kAction});
  std::string serialized = info.ToDataElementBytes();
  auto result = BluetoothConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_OK(result);
  EXPECT_EQ(result.value(), info);
}

TEST(BluetoothConnectionInfoTest, TestFromEmptyBytes) {
  auto result = BluetoothConnectionInfo::FromDataElementBytes("");
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(BluetoothConnectionInfoTest, TestFromNoAction) {
  BluetoothConnectionInfo info("", "", {kAction});
  std::string serialized = info.ToDataElementBytes();
  serialized[1] -= 1;
  auto result = BluetoothConnectionInfo::FromDataElementBytes(
      serialized.substr(0, serialized.length() - 1));
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(BluetoothConnectionInfoTest, TestFromInvalidBytes) {
  auto result = BluetoothConnectionInfo::FromDataElementBytes(kMacAddr);
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(BluetoothConnectionInfoTest, TestFromBadElementType) {
  BluetoothConnectionInfo info(kMacAddr, kBluetoothUuid, {kAction});
  std::string serialized = info.ToDataElementBytes();
  serialized[0] = 0x56;
  auto result = BluetoothConnectionInfo::FromDataElementBytes(serialized);
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(BluetoothConnectionInfoTest, TestCopy) {
  BluetoothConnectionInfo info(kMacAddr, kBluetoothUuid, {kAction});
  BluetoothConnectionInfo copy(info);
  EXPECT_EQ(info, copy);
}

TEST(BluetoothConnectionInfoTest, TestEquals) {
  BluetoothConnectionInfo info(kMacAddr, kBluetoothUuid, {kAction});
  BluetoothConnectionInfo info2(kMacAddr, kBluetoothUuid, {kAction});
  EXPECT_EQ(info, info2);
}

}  // namespace
}  // namespace nearby
