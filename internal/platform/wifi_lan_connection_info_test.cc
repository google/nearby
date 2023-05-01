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

#include "internal/platform/wifi_lan_connection_info.h"

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

constexpr absl::string_view kIpv4Addr = "\x4C\x8B\x1D\xCE";
constexpr absl::string_view kIpv6Addr =
    "\x4C\x8B\x1D\xCE\x4C\x8B\x1D\xCE\x4C\x8B\x1D\xCE\x4C\x8B\x1D\xCE";
constexpr absl::string_view kPort = "\x12\x34";
constexpr absl::string_view kBssid = "\x0A\x1B\x2C\x34\x58\x7E";
constexpr char kAction = 0x0F;

using ::testing::status::StatusIs;
using Medium = ::location::nearby::proto::connections::Medium;

TEST(WifiLanConnectionInfoTest, TestMediumType) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, {kAction});
  EXPECT_EQ(info.GetMediumType(), Medium::WIFI_LAN);
}

TEST(WifiLanConnectionInfoTest, TestGetMembers) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, kBssid, {kAction});
  EXPECT_EQ(info.GetIpAddress(), kIpv4Addr);
  EXPECT_EQ(info.GetPort(), kPort);
  EXPECT_EQ(info.GetBssid(), kBssid);
  ASSERT_EQ(info.GetActions().size(), 1);
  EXPECT_EQ(info.GetActions()[0], kAction);
}

TEST(WifiLanConnectionInfoTest, TestToFromBytesIpv4) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, kBssid, {kAction});
  std::string serialized = info.ToDataElementBytes();
  auto result = WifiLanConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_OK(result);
  EXPECT_EQ(result.value(), info);
}

TEST(WifiLanConnectionInfoTest, TestToFromBytesIpv6) {
  WifiLanConnectionInfo info(kIpv6Addr, kPort, kBssid, {kAction});
  std::string serialized = info.ToDataElementBytes();
  auto result = WifiLanConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_OK(result);
  EXPECT_EQ(result.value(), info);
}

TEST(WifiLanConnectionInfoTest, TestCopy) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, kBssid, {kAction});
  WifiLanConnectionInfo copy(info);
  EXPECT_EQ(info, copy);
}

TEST(WifiLanConnectionInfoTest, TestEquals) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, kBssid, {kAction});
  WifiLanConnectionInfo info2(kIpv4Addr, kPort, kBssid, {kAction});
  EXPECT_EQ(info, info2);
}

TEST(WifiLanConnectionInfoTest, TestFromNoAction) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, kBssid, {kAction});
  std::string serialized = info.ToDataElementBytes();
  serialized[1] -= 1;
  auto result = WifiLanConnectionInfo::FromDataElementBytes(
      serialized.substr(0, serialized.length() - 1));
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(WifiLanConnectionInfoTest, TestToFromShortBssid) {
  std::string shortBssid = "\x0A\x1B\x2C";
  WifiLanConnectionInfo info(kIpv4Addr, kPort, shortBssid, {kAction});
  auto bytes = info.ToDataElementBytes();
  auto result = WifiLanConnectionInfo::FromDataElementBytes(bytes);
  ASSERT_OK(result);
  EXPECT_EQ(result->GetIpAddress(), kIpv4Addr);
  EXPECT_EQ(result->GetPort(), kPort);
  EXPECT_TRUE(result->GetBssid().empty());
  ASSERT_EQ(result->GetActions().size(), 1);
  EXPECT_EQ(result->GetActions()[0], kAction);
}

TEST(WifiLanConnectionInfoTest, TestToFromNoIp) {
  WifiLanConnectionInfo info("", kPort, kBssid, {kAction});
  auto bytes = info.ToDataElementBytes();
  auto result = WifiLanConnectionInfo::FromDataElementBytes(bytes);
  ASSERT_OK(result);
  EXPECT_TRUE(result->GetIpAddress().empty());
  EXPECT_EQ(result->GetPort(), kPort);
  EXPECT_EQ(result->GetBssid(), kBssid);
  ASSERT_EQ(result->GetActions().size(), 1);
  EXPECT_EQ(result->GetActions()[0], kAction);
}

TEST(WifiLanConnectionInfoTest, TestToFromLongIp) {
  WifiLanConnectionInfo info(absl::StrCat(kIpv4Addr, kIpv6Addr), kPort, kBssid,
                             {kAction});
  auto bytes = info.ToDataElementBytes();
  auto result = WifiLanConnectionInfo::FromDataElementBytes(bytes);
  ASSERT_OK(result);
  EXPECT_TRUE(result->GetIpAddress().empty());
  EXPECT_EQ(result->GetPort(), kPort);
  EXPECT_EQ(result->GetBssid(), kBssid);
  ASSERT_EQ(result->GetActions().size(), 1);
  EXPECT_EQ(result->GetActions()[0], kAction);
}

TEST(WifiLanConnectionInfoTest, TestFromIp) {
  auto result = WifiLanConnectionInfo::FromDataElementBytes(kIpv4Addr);
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
  result = WifiLanConnectionInfo::FromDataElementBytes(kIpv6Addr);
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(WifiLanConnectionInfoTest, TestBadBytesLength) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, kBssid, {kAction});
  std::string serialized = info.ToDataElementBytes();
  std::string modified_short(
      serialized.substr(0, kIpv4AddressLength + kPortLength));
  std::string modified_long(absl::StrCat(serialized, kPort));
  EXPECT_THAT(WifiLanConnectionInfo::FromDataElementBytes(modified_short),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(WifiLanConnectionInfo::FromDataElementBytes(""),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(WifiLanConnectionInfo::FromDataElementBytes(modified_long),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(WifiLanConnectionInfoTest, TestFromBadElementType) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, kBssid, {kAction});
  std::string serialized = info.ToDataElementBytes();
  serialized[0] = 0x56;
  auto result = WifiLanConnectionInfo::FromDataElementBytes(serialized);
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace nearby
