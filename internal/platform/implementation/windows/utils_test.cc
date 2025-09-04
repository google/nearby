// Copyright 2020-2025 Google LLC
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

#include "internal/platform/implementation/windows/utils.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {
namespace {
using ::winrt::Windows::Foundation::IInspectable;
using ::winrt::Windows::Foundation::PropertyValue;

constexpr absl::string_view kIpDotdecimal{"192.168.1.37"};
constexpr char kIp4Bytes[] = {(char)192, (char)168, (char)1, (char)37};

}  // namespace

TEST(UtilsTests, Ip4BytesToDotdecimal) {
  std::string result =
      ipaddr_4bytes_to_dotdecimal_string(absl::string_view(kIp4Bytes, 4));

  EXPECT_EQ(result, kIpDotdecimal);
}

TEST(UtilsTests, Ip4BytesToDotdecimalInvalid) {
  std::string result = ipaddr_4bytes_to_dotdecimal_string(absl::string_view());
  EXPECT_TRUE(result.empty());
}

TEST(UtilsTests, IpDotdecimalTo4Bytes) {
  std::string result =
      ipaddr_dotdecimal_to_4bytes_string(std::string(kIpDotdecimal));

  EXPECT_EQ(result, std::string(kIp4Bytes, 4));
}

TEST(UtilsTests, IpDotdecimalTo4BytesEmpty) {
  std::string result = ipaddr_dotdecimal_to_4bytes_string("");
  EXPECT_TRUE(result.empty());
}

TEST(UtilsTests, IpDotdecimalTo4BytesInvalid) {
  std::string result = ipaddr_dotdecimal_to_4bytes_string("192.168.1.256");
  // inet_addr returns INADDR_NONE for invalid address.
  char expected[] = {(char)255, (char)255, (char)255, (char)255};
  EXPECT_EQ(result, std::string(expected, 4));
}

TEST(UtilsTests, Sha256) {
  std::string input = "Hello World";
  // sha256("Hello World")
  const char expected_sha256[] = {
      (char)0xa5, (char)0x91, (char)0xa6, (char)0xd4, (char)0x0b, (char)0xf4,
      (char)0x20, (char)0x40, (char)0x4a, (char)0x01, (char)0x17, (char)0x33,
      (char)0xcf, (char)0xb7, (char)0xb1, (char)0x90, (char)0xd6, (char)0x2c,
      (char)0x65, (char)0xbf, (char)0x0b, (char)0xcd, (char)0xa3, (char)0x2b,
      (char)0x57, (char)0xb2, (char)0x77, (char)0xd9, (char)0xad, (char)0x9f,
      (char)0x14, (char)0x6e};

  ByteArray result = Sha256(input, 32);
  EXPECT_EQ(result.size(), 32);
  EXPECT_EQ(memcmp(result.data(), expected_sha256, 32), 0);

  result = Sha256(input, 16);
  EXPECT_EQ(result.size(), 16);
  EXPECT_EQ(memcmp(result.data(), expected_sha256, 16), 0);
}

TEST(UtilsTests, ConvertBetweenWinrtGuidAndNearbyUuidSuccessfully) {
  Uuid uuid(0x123e4567e89b12d3, 0xa456426614174000);
  winrt::guid guid("{123e4567-e89b-12d3-a456-426614174000}");

  EXPECT_EQ(uuid, winrt_guid_to_nearby_uuid(guid));
  EXPECT_EQ(nearby_uuid_to_winrt_guid(uuid), guid);
  EXPECT_TRUE(is_nearby_uuid_equal_to_winrt_guid(uuid, guid));
}

TEST(UtilsTests, CompareWinrtGuidAndNearbyUuidSuccessfully) {
  Uuid uuid(0x123e4567e89b12d3, 0xa456426614174000);
  winrt::guid guid("123e4567-e89b-12d3-a456-426614074000");

  EXPECT_NE(uuid, winrt_guid_to_nearby_uuid(guid));
}

TEST(UtilsTests, InspectableReader_ReadBoolean) {
  EXPECT_TRUE(
      InspectableReader::ReadBoolean(PropertyValue::CreateBoolean(true)));
  EXPECT_FALSE(
      InspectableReader::ReadBoolean(PropertyValue::CreateBoolean(false)));
  EXPECT_FALSE(InspectableReader::ReadBoolean(nullptr));
  EXPECT_THROW(InspectableReader::ReadBoolean(PropertyValue::CreateString(L"")),
               std::invalid_argument);
}

TEST(UtilsTests, InspectableReader_ReadUint16) {
  EXPECT_EQ(InspectableReader::ReadUint16(PropertyValue::CreateUInt16(123)),
            123);
  EXPECT_EQ(InspectableReader::ReadUint16(nullptr), 0);
  EXPECT_THROW(InspectableReader::ReadUint16(PropertyValue::CreateString(L"")),
               std::invalid_argument);
}

TEST(UtilsTests, InspectableReader_ReadUint32) {
  EXPECT_EQ(InspectableReader::ReadUint32(PropertyValue::CreateUInt32(456)),
            456);
  EXPECT_EQ(InspectableReader::ReadUint32(nullptr), 0);
  EXPECT_THROW(InspectableReader::ReadUint32(PropertyValue::CreateString(L"")),
               std::invalid_argument);
}

TEST(UtilsTests, InspectableReader_ReadString) {
  EXPECT_EQ(InspectableReader::ReadString(
                PropertyValue::CreateString(L"test string")),
            "test string");
  EXPECT_EQ(InspectableReader::ReadString(nullptr), "");
  EXPECT_THROW(
      InspectableReader::ReadString(PropertyValue::CreateBoolean(true)),
      std::invalid_argument);
}

TEST(UtilsTests, InspectableReader_ReadStringArray) {
  winrt::com_array<winrt::hstring> string_array = {L"a", L"b", L"c"};
  std::vector<std::string> expected = {"a", "b", "c"};
  IInspectable inspectable = PropertyValue::CreateStringArray(string_array);
  EXPECT_EQ(InspectableReader::ReadStringArray(inspectable), expected);
  EXPECT_TRUE(InspectableReader::ReadStringArray(nullptr).empty());
  EXPECT_THROW(
      InspectableReader::ReadStringArray(PropertyValue::CreateBoolean(true)),
      std::invalid_argument);
}

TEST(UtilsTests, GetIpv4Addresses) {
  LOG(ERROR) << "GetIpv4Addresses";
  std::vector<std::string> addresses = GetIpv4Addresses();
  EXPECT_FALSE(addresses.empty());
  for (const auto& address : addresses) {
    LOG(ERROR) << "address: " << address;
  }
  LOG(ERROR) << "GetIpv4Addresses done";
}

TEST(UtilsTests, GetDnsHostName) {
  std::optional<std::wstring> host_name = GetDnsHostName();
  ASSERT_TRUE(host_name.has_value());
  LOG(ERROR) << "host_name: "
             << nearby::windows::string_utils::WideStringToString(*host_name);
}

}  // namespace windows
}  // namespace nearby
