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

}  // namespace

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

TEST(UtilsTests, GetDnsHostName) {
  std::optional<std::wstring> host_name = GetDnsHostName();
  ASSERT_TRUE(host_name.has_value());
  LOG(ERROR) << "host_name: "
             << nearby::windows::string_utils::WideStringToString(*host_name);
}

TEST(UtilsTests, IsIntelWifiAdapter) {
  bool is_intel_wifi_adapter = IsIntelWifiAdapter();
  LOG(ERROR) << "is_intel_wifi_adapter: " << is_intel_wifi_adapter;
}

}  // namespace windows
}  // namespace nearby
