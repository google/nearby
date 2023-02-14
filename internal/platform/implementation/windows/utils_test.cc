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

}  // namespace windows
}  // namespace nearby
