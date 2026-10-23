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

#include "internal/platform/byte_utils.h"

#include <cstdint>
#include <string>
#include <limits>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace nearby {

constexpr absl::string_view kFooBytes{"rawABCDE"};
constexpr absl::string_view kFooFourDigitsToken{"0392"};
constexpr absl::string_view kEmptyFourDigitsToken{"0000"};
constexpr absl::string_view kNegativeBytes{"raw\xd5\x01\xe4\x03\x81"};
constexpr absl::string_view kNegativeFourDigitsToken{"6251"};

TEST(ByteUtilsTest, ToFourDigitStringCorrect) {
  ByteArray bytes{std::string(kFooBytes)};

  auto four_digit_string = byte_utils::ToFourDigitString(bytes);

  EXPECT_EQ(std::string(kFooFourDigitsToken), four_digit_string);
}

TEST(ByteUtilsTest, ToFourDigitStringNegativeCorrect) {
  ByteArray bytes{std::string(kNegativeBytes)};

  auto four_digit_string = byte_utils::ToFourDigitString(bytes);

  EXPECT_EQ(std::string(kNegativeFourDigitsToken), four_digit_string);
}

TEST(ByteUtilsTest, ToFourDigitStringHandlesEmptyInput) {
  ByteArray bytes{};

  auto four_digit_string = byte_utils::ToFourDigitString(bytes);

  EXPECT_EQ(std::string(kEmptyFourDigitsToken), four_digit_string);
}

TEST(ByteUtilsTest, IntToBytesThenBytesToIntIsSymmetric) {
  constexpr std::int32_t kTestValue = 123456789;

  ByteArray bytes = byte_utils::IntToBytes(kTestValue);

  EXPECT_EQ(kTestValue, byte_utils::BytesToInt(bytes));
}

TEST(ByteUtilsTest, IntToBytesThenBytesToIntIsSymmetricNegative) {
  constexpr std::int32_t kTestValue = -123456789;

  ByteArray bytes = byte_utils::IntToBytes(kTestValue);

  EXPECT_EQ(kTestValue, byte_utils::BytesToInt(bytes));
}

TEST(ByteUtilsTest, BytesToIntHandlesZero) {
  ByteArray bytes({'\0', '\0', '\0', '\0'});
  EXPECT_EQ(0, byte_utils::BytesToInt(bytes));
}

TEST(ByteUtilsTest, BytesToIntHandlesNegative) {
  ByteArray bytes({'\xff', '\xff', '\xff', '\xff'});
  EXPECT_EQ(-1, byte_utils::BytesToInt(bytes));
}

TEST(ByteUtilsTest, BytesToIntHandlesNegativeLarge) {
  ByteArray bytes({'\x80', '\x00', '\x00', '\x00'});
  EXPECT_EQ(std::numeric_limits<std::int32_t>::min(),
            byte_utils::BytesToInt(bytes));
}

TEST(ByteUtilsTest, IntToBytesHandlesZero) {
  ByteArray expected_bytes({'\0', '\0', '\0', '\0'});
  EXPECT_EQ(expected_bytes, byte_utils::IntToBytes(0));
}

TEST(ByteUtilsTest, HandlesInt32Max) {
  constexpr std::int32_t kMaxValue = std::numeric_limits<std::int32_t>::max();

  ByteArray bytes = byte_utils::IntToBytes(kMaxValue);

  EXPECT_EQ(kMaxValue, byte_utils::BytesToInt(bytes));
}

TEST(ByteUtilsTest, HandlesInt32Min) {
  constexpr std::int32_t kMinValue = std::numeric_limits<std::int32_t>::min();

  ByteArray bytes = byte_utils::IntToBytes(kMinValue);

  EXPECT_EQ(kMinValue, byte_utils::BytesToInt(bytes));
}

}  // namespace nearby
