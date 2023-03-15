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

#include "internal/flags/nearby_flags.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "internal/flags/flag.h"
#include "internal/flags/flag_reader.h"

namespace nearby {
namespace {

constexpr auto kTestBoolFlag =
    flags::Flag<bool>("test_package", "45401515", false);
constexpr auto kTestInt64Flag =
    flags::Flag<int64_t>("test_package", "45401516", 666);
constexpr auto kTestDoubleFlag =
    flags::Flag<double>("test_package", "45401517", 66.66);
constexpr auto kTestStringFlag =
    flags::Flag<absl::string_view>("test_package", "45401518", "string");

constexpr bool kTestBoolFlagTestValue = true;
constexpr int64_t kTestInt64FlagTestValue = 666;
constexpr double kTestDoubleFlagTestValue = 88.8;
constexpr std::string_view kTestStringFlagTestValue = "hello";

class MockFlagReader : public flags::FlagReader {
 public:
  MOCK_METHOD(bool, GetBoolFlag, (const flags::Flag<bool>& flag), (override));
  MOCK_METHOD(int64_t, GetInt64Flag, (const flags::Flag<int64_t>& flag),
              (override));
  MOCK_METHOD(double, GetDoubleFlag, (const flags::Flag<double>& flag),
              (override));
  MOCK_METHOD(std::string, GetStringFlag,
              (const flags::Flag<absl::string_view>& flag), (override));
};

TEST(NearbyFlags, GetDefaultValues) {
  EXPECT_EQ(NearbyFlags::GetInstance().GetBoolFlag(kTestBoolFlag),
            kTestBoolFlag.default_value());
  EXPECT_EQ(NearbyFlags::GetInstance().GetInt64Flag(kTestInt64Flag),
            kTestInt64Flag.default_value());
  EXPECT_EQ(NearbyFlags::GetInstance().GetDoubleFlag(kTestDoubleFlag),
            kTestDoubleFlag.default_value());
  EXPECT_EQ(NearbyFlags::GetInstance().GetStringFlag(kTestStringFlag),
            kTestStringFlag.default_value());
}

TEST(NearbyFlags, OverrideDefaultValues) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(kTestBoolFlag,
                                                   kTestBoolFlagTestValue);
  EXPECT_EQ(NearbyFlags::GetInstance().GetBoolFlag(kTestBoolFlag),
            kTestBoolFlagTestValue);
  NearbyFlags::GetInstance().OverrideInt64FlagValue(kTestInt64Flag,
                                                    kTestInt64FlagTestValue);
  EXPECT_EQ(NearbyFlags::GetInstance().GetInt64Flag(kTestInt64Flag),
            kTestInt64FlagTestValue);
  NearbyFlags::GetInstance().OverrideDoubleFlagValue(kTestDoubleFlag,
                                                     kTestDoubleFlagTestValue);
  EXPECT_EQ(NearbyFlags::GetInstance().GetDoubleFlag(kTestDoubleFlag),
            kTestDoubleFlagTestValue);
  NearbyFlags::GetInstance().OverrideStringFlagValue(kTestStringFlag,
                                                     kTestStringFlagTestValue);
  EXPECT_EQ(NearbyFlags::GetInstance().GetStringFlag(kTestStringFlag),
            kTestStringFlagTestValue);
  NearbyFlags::GetInstance().ResetOverridedValues();
}

TEST(NearbyFlags, SetFlagReader) {
  auto flag_reader = std::make_unique<::testing::NiceMock<MockFlagReader>>();
  NearbyFlags::GetInstance().SetFlagReader(*flag_reader.get());
  EXPECT_CALL(*flag_reader, GetBoolFlag(::testing::_))
      .WillOnce(::testing::Invoke([=](const flags::Flag<bool>& flag) {
        return kTestBoolFlagTestValue;
      }));
  EXPECT_EQ(NearbyFlags::GetInstance().GetBoolFlag(kTestBoolFlag),
            kTestBoolFlagTestValue);
  EXPECT_CALL(*flag_reader, GetInt64Flag(::testing::_))
      .WillOnce(::testing::Invoke([=](const flags::Flag<int64_t>& flag) {
        return kTestInt64FlagTestValue;
      }));
  EXPECT_EQ(NearbyFlags::GetInstance().GetInt64Flag(kTestInt64Flag),
            kTestInt64FlagTestValue);
  EXPECT_CALL(*flag_reader, GetDoubleFlag(::testing::_))
      .WillOnce(::testing::Invoke([=](const flags::Flag<double>& flag) {
        return kTestDoubleFlagTestValue;
      }));
  EXPECT_EQ(NearbyFlags::GetInstance().GetDoubleFlag(kTestDoubleFlag),
            kTestDoubleFlagTestValue);
  EXPECT_CALL(*flag_reader, GetStringFlag(::testing::_))
      .WillOnce(
          ::testing::Invoke([=](const flags::Flag<absl::string_view>& flag) {
            return std::string(kTestStringFlagTestValue);
          }));
  EXPECT_EQ(NearbyFlags::GetInstance().GetStringFlag(kTestStringFlag),
            kTestStringFlagTestValue);
}

}  // namespace
}  // namespace nearby
