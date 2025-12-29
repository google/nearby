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

#include <string>

#include "absl/strings/ascii.h"
#include "internal/platform/implementation/linux/utils.h"

#include "gtest/gtest.h"

namespace nearby {
namespace linux {

TEST(UtilsTests, UuidFromStringRoundTrip) {
  std::string input = "b5209043-f493-4b38-8c34-810aa3cd1407";

  auto nearby_uuid = UuidFromString(input);
  EXPECT_TRUE(nearby_uuid.has_value());

  EXPECT_EQ(absl::AsciiStrToLower(std::string{*nearby_uuid}),
            "b5209043-f493-4b38-8c34-810aa3cd1407");
}

TEST(UtilsTests, GenNewUuid) {
  auto uuid_str = NewUuidStr();
  EXPECT_TRUE(uuid_str.has_value());
  auto uuid = UuidFromString(*uuid_str);
  EXPECT_TRUE(uuid.has_value());
  EXPECT_EQ(absl::AsciiStrToLower(std::string{*uuid}),
            *uuid_str);
}

TEST(UtilsTests, GenRandSSID) {
  std::string ssid = RandSSID();
  EXPECT_EQ(ssid.length(), 32);
  EXPECT_EQ(ssid.find("DIRECT-"), 0);
}

TEST(UtilsTests, GenRandRandWPAPassphrase) {
  std::string password = RandWPAPassphrase();
  EXPECT_EQ(password.length(), 63);
}
}  // namespace linux
}  // namespace nearby
