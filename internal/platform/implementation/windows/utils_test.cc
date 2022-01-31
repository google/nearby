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

#include "gtest/gtest.h"

using location::nearby::windows::uint64_to_mac_address_string;

TEST(UtilsTests, MacAddressToString) {
  // Arrange
  const uint64_t input = 0x000034363bc78c71;
  std::string expected = "34:36:3B:C7:8C:71";

  // Act
  std::string result = uint64_to_mac_address_string(input);

  // Assert
  EXPECT_EQ(result, expected);
}
