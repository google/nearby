// Copyright 2020 Google LLC
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

#include <sstream>
#include <string>

#include "gtest/gtest.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {

TEST(UtilsTests, MacAddressToString) {
  // Arrange
  const uint64_t input = 0x000034363bc70c71;
  std::string expected = "34:36:3B:C7:0C:71";

  // Act
  std::string result = uint64_to_mac_address_string(input);

  // Assert
  EXPECT_EQ(result, expected);
}

TEST(UtilsTests, StringToMacAddress) {
  // Arrange
  std::string input = "34:36:3B:C7:8C:71";
  const uint64_t expected = 0x000034363bc78c71;

  // Act
  uint64_t result = mac_address_string_to_uint64(input);

  // Assert
  EXPECT_EQ(result, expected);
}

constexpr absl::string_view kIpDotdecimal{"192.168.1.37"};

constexpr char kIp4Bytes[] = {(char)192, (char)168, (char)1, (char)37};

TEST(UtilsTests, Ip4BytesToDotdecimal) {
  std::string result =
      ipaddr_4bytes_to_dotdecimal_string(absl::string_view(kIp4Bytes));

  EXPECT_EQ(result, kIpDotdecimal);
}

TEST(UtilsTests, IpDotdecimalTo4Bytes) {
  std::string result =
      ipaddr_dotdecimal_to_4bytes_string(std::string(kIpDotdecimal));

  EXPECT_EQ(result, std::string(kIp4Bytes, 4));
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

const wchar_t* const kConvertRoundtripCases[] = {
    L"Nearby Share for Windows",
    // "附近分享 蓝牙 无线传送 »"
    L"\x644\x8fd1\x5206\x4eab\x0020\x84dd\x7259\x0020\x65e0\x7ebf\x4f20\x9001"
    L"\x0020\x00bb",
    // "近隣のシェア"
    L"\x8fd1\x96a3\x306e\x30b7\x30a7\x30a2"
    // "주변 공유"
    L"\xc8fc\xbcc0\x0020\xacf5\xc720"
    // "आस-पास साझा करें"
    L"\x0906\x0938\x002d\x092a\x093e\x0938\x0020\x0938\x093e\x091d\x093e\x0020"
    L"\x0915\x0930\x0947\x0902",
    // "அருகிலுள்ள பகிர்வு"
    L"\x0b85\x0bb0\x0bc1\x0b95\x0bbf\x0bb2\x0bc1\x0bb3\x0bcd\x0bb3\x0020\x0baa"
    L"\x0b95\x0bbf\x0bb0\x0bcd\x0bb5\x0bc1",
    // ?????  (Mathematical Alphanumeric Symbols (U+011d40 - U+011d44 :
    // A,B,C,D,E)
    L"\xd807\xdd40\xd807\xdd41\xd807\xdd42\xd807\xdd43\xd807\xdd44",
};

TEST(UtilsTests, ConvertStringAndWideString) {
  // we round-trip all the wide strings through UTF-8 to make sure everything
  // agrees on the conversion. This uses the stream operators to test them
  // simultaneously.
  for (auto* i : kConvertRoundtripCases) {
    std::ostringstream utf8;
    utf8 << wstring_to_string(i);
    std::wostringstream wide;
    wide << string_to_wstring(utf8.str());

    EXPECT_EQ(i, wide.str());
  }
}

TEST(UtilsTests, ConvertEmptyStringAndWideString) {
  // An empty std::wstring should be converted to an empty std::string,
  // and vice versa.
  std::wstring wide_empty;
  std::string empty;
  EXPECT_EQ(empty, wstring_to_string(wide_empty));
  EXPECT_EQ(wide_empty, string_to_wstring(empty));
}

}  // namespace windows
}  // namespace nearby
