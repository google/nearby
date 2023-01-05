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

#include "internal/platform/wifi_lan_connection_info.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace nearby {
namespace {

constexpr absl::string_view kIpv4Addr = "\x4C\x8B\x1D\xCE";
constexpr absl::string_view kIpv6Addr =
    "\x4C\x8B\x1D\xCE\x4C\x8B\x1D\xCE\x4C\x8B\x1D\xCE\x4C\x8B\x1D\xCE";
constexpr absl::string_view kPort = "\x56\x78";
constexpr absl::string_view kBssid = "\x0A\x1B\x2C\x34\x58\x7E";

using ::testing::status::StatusIs;

TEST(WifiLanConnectionInfoTest, TestMediumType) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort);
  EXPECT_EQ(info.GetMediumType(), WifiLanConnectionInfo::MediumType::kWifiLan);
}

TEST(WifiLanConnectionInfoTest, TestGetMembers) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, kBssid);
  EXPECT_EQ(info.GetIpAddress().AsStringView(), kIpv4Addr);
  EXPECT_EQ(info.GetPort().AsStringView(), kPort);
  EXPECT_EQ(info.GetBssid().AsStringView(), kBssid);
}

TEST(WifiLanConnectionInfoTest, TestIpv4ToBytes) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, kBssid);
  ByteArray expected(
      absl::StrCat(std::to_string(info.GetWifiLanConnectionInfoType()),
                   kIpv4Addr, kPort, kBssid));
  EXPECT_EQ(info.ToBytes(), expected);
}

TEST(WifiLanConnectionInfoTest, TestFromBytesIpv4) {
  ByteArray expected = ByteArray(absl::StrCat(
      std::to_string(WifiLanConnectionInfo::kIpv4), kIpv4Addr, kPort, kBssid));
  ASSERT_OK(WifiLanConnectionInfo::FromBytes(expected));
  WifiLanConnectionInfo info =
      WifiLanConnectionInfo::FromBytes(expected).value();
  EXPECT_EQ(info.GetIpAddress().AsStringView(), kIpv4Addr);
  EXPECT_EQ(info.GetPort().AsStringView(), kPort);
  EXPECT_EQ(info.GetBssid().AsStringView(), kBssid);
}

TEST(WifiLanConnectionInfoTest, TestFromBytesIpv6) {
  ByteArray expected = ByteArray(absl::StrCat(
      std::to_string(WifiLanConnectionInfo::kIpv6), kIpv6Addr, kPort, kBssid));
  ASSERT_OK(WifiLanConnectionInfo::FromBytes(expected));
  WifiLanConnectionInfo info =
      WifiLanConnectionInfo::FromBytes(expected).value();
  EXPECT_EQ(info.GetIpAddress().AsStringView(), kIpv6Addr);
  EXPECT_EQ(info.GetPort().AsStringView(), kPort);
  EXPECT_EQ(info.GetBssid().AsStringView(), kBssid);
}

TEST(WifiLanConnectionInfoTest, TestToFromBytesIpv4) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, kBssid);
  ByteArray serialized = info.ToBytes();
  WifiLanConnectionInfo result =
      WifiLanConnectionInfo::FromBytes(serialized).value();
  EXPECT_EQ(result, info);
}

TEST(WifiLanConnectionInfoTest, TestToFromBytesIpv6) {
  WifiLanConnectionInfo info(kIpv6Addr, kPort, kBssid);
  ByteArray serialized = info.ToBytes();
  WifiLanConnectionInfo result =
      WifiLanConnectionInfo::FromBytes(serialized).value();
  EXPECT_EQ(result, info);
}

TEST(WifiLanConnectionInfoTest, TestCopy) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, kBssid);
  WifiLanConnectionInfo copy(info);
  EXPECT_EQ(info, copy);
}

TEST(WifiLanConnectionInfoTest, TestEquals) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort, kBssid);
  WifiLanConnectionInfo info2(kIpv4Addr, kPort, kBssid);
  EXPECT_EQ(info, info2);
}

TEST(WifiLanConnectionInfoTest, TestShortBssid) {
  std::string shortBssid = "\x0A\x1B\x2C";
  std::string extended("\x0A\x1B\x2C\0\0\0", kBssidLength);
  WifiLanConnectionInfo info(kIpv4Addr, kPort, shortBssid);
  EXPECT_EQ(info.GetBssid().size(), kBssidLength);
  // Value-initialized so there will be 3 0x0 characters
  EXPECT_EQ(info.GetBssid().AsStringView(), extended);
}

TEST(WifiLanConnectionInfoTest, TestLongBssid) {
  std::string longBssid = absl::StrCat(kBssid, "\x68\x42\x35");
  WifiLanConnectionInfo info(kIpv4Addr, kPort, longBssid);
  EXPECT_EQ(info.GetBssid().size(), kBssidLength);
  EXPECT_EQ(info.GetBssid().AsStringView(), kBssid);
}

TEST(WifiLanConnectionInfoTest, TestBadBytesLength) {
  WifiLanConnectionInfo info(kIpv4Addr, kPort);
  ByteArray serialized = info.ToBytes();
  ByteArray modified_short(std::string(
      serialized.AsStringView().substr(0, kIpv4AddressLength + kPortLength)));
  ByteArray modified_long(absl::StrCat(serialized.AsStringView(), kPort));
  EXPECT_THAT(WifiLanConnectionInfo::FromBytes(modified_short),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(WifiLanConnectionInfo::FromBytes(ByteArray()),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(WifiLanConnectionInfo::FromBytes(modified_long),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace nearby
