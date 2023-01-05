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

#include "gtest/gtest.h"
#include "internal/platform/byte_array.h"

namespace nearby {

constexpr absl::string_view kFooBytes{"rawABCDE"};
constexpr absl::string_view kFooFourDigitsToken{"0392"};
constexpr absl::string_view kEmptyFourDigitsToken{"0000"};

TEST(ByteUtilsTest, ToFourDigitStringCorrect) {
  ByteArray bytes{std::string(kFooBytes)};

  auto four_digit_string = ByteUtils::ToFourDigitString(bytes);

  EXPECT_EQ(std::string(kFooFourDigitsToken), four_digit_string);
}

TEST(ByteUtilsTest, TestEmptyByteArrayCorrect) {
  ByteArray bytes;

  auto four_digit_string = ByteUtils::ToFourDigitString(bytes);

  EXPECT_EQ(std::string(kEmptyFourDigitsToken), four_digit_string);
}

}  // namespace nearby
