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

namespace {

struct FakeRevoker {
  bool* revoked = nullptr;
  int exception_type = 0;  // 0 = none, 1 = hresult, 2 = std::exception, 3 = int

  explicit FakeRevoker(bool* revoked, int exception_type = 0)
      : revoked(revoked), exception_type(exception_type) {}

  FakeRevoker(const FakeRevoker&) = delete;
  FakeRevoker& operator=(const FakeRevoker&) = delete;

  FakeRevoker(FakeRevoker&& other) noexcept
      : revoked(other.revoked), exception_type(other.exception_type) {
    other.revoked = nullptr;
  }

  ~FakeRevoker() {
    if (*this) {
      revoke();
    }
  }

  void revoke() {
    if (revoked != nullptr) {
      *revoked = true;
      revoked = nullptr;
    }
    if (exception_type == 1) {
      throw winrt::hresult_error(E_FAIL, L"Simulated WinRT error");
    } else if (exception_type == 2) {
      throw std::runtime_error("Simulated std::exception");
    } else if (exception_type == 3) {
      throw 42;
    }
  }

  explicit operator bool() const noexcept { return revoked != nullptr; }
};

}  // namespace

TEST(UtilsTests, SafeAutoRevoker_RevokesOnDestruction) {
  bool revoked = false;
  {
    SafeAutoRevoker<FakeRevoker> safe_revoker{FakeRevoker(&revoked)};
    EXPECT_FALSE(revoked);
  }
  EXPECT_TRUE(revoked);
}

TEST(UtilsTests, SafeAutoRevoker_DoesNotRevokeWhenEmpty) {
  EXPECT_NO_THROW(
      { SafeAutoRevoker<FakeRevoker> safe_revoker{FakeRevoker(nullptr, 1)}; });
}

TEST(UtilsTests, SafeAutoRevoker_IgnoresExceptions) {
  bool revoked1 = false;
  bool revoked2 = false;
  bool revoked3 = false;
  EXPECT_NO_THROW({
    SafeAutoRevoker<FakeRevoker> safe_revoker1{FakeRevoker(&revoked1, 1)};
  });
  EXPECT_TRUE(revoked1);

  EXPECT_NO_THROW({
    SafeAutoRevoker<FakeRevoker> safe_revoker2{FakeRevoker(&revoked2, 2)};
  });
  EXPECT_TRUE(revoked2);

  EXPECT_NO_THROW({
    SafeAutoRevoker<FakeRevoker> safe_revoker3{FakeRevoker(&revoked3, 3)};
  });
  EXPECT_TRUE(revoked3);
}

}  // namespace windows
}  // namespace nearby
