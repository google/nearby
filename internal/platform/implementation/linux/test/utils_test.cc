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

#include "internal/platform/implementation/linux/utils.h"

#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace linux {
namespace {

TEST(UtilsTests, UuidFromStringValid) {
  std::string uuid_str = "123e4567-e89b-12d3-a456-426614174000";
  std::optional<Uuid> uuid = UuidFromString(uuid_str);
  ASSERT_TRUE(uuid.has_value());
  EXPECT_EQ(uuid->GetLeastSigBits(), 0xa456426614174000ULL);
  EXPECT_EQ(uuid->GetMostSigBits(), 0x123e4567e89b12d3ULL);
}

TEST(UtilsTests, UuidFromStringInvalid) {
  std::string invalid_uuid_str = "invalid-uuid-string";
  std::optional<Uuid> uuid = UuidFromString(invalid_uuid_str);
  
  EXPECT_FALSE(uuid.has_value());
}

TEST(UtilsTests, UuidFromStringEmpty) {
  std::string empty_uuid_str = "";
  std::optional<Uuid> uuid = UuidFromString(empty_uuid_str);
  
  EXPECT_FALSE(uuid.has_value());
}

TEST(UtilsTests, NewUuidStr) {
  std::optional<std::string> uuid_str = NewUuidStr();
  
  ASSERT_TRUE(uuid_str.has_value());
  EXPECT_FALSE(uuid_str->empty());
  EXPECT_EQ(uuid_str->length(), 36);  // Standard UUID string length
  
  // Verify it can be parsed back
  std::optional<Uuid> uuid = UuidFromString(*uuid_str);
  EXPECT_TRUE(uuid.has_value());
}

TEST(UtilsTests, NewUuidStrUnique) {
  std::optional<std::string> uuid_str1 = NewUuidStr();
  std::optional<std::string> uuid_str2 = NewUuidStr();
  
  ASSERT_TRUE(uuid_str1.has_value());
  ASSERT_TRUE(uuid_str2.has_value());
  EXPECT_NE(*uuid_str1, *uuid_str2);
}

TEST(UtilsTests, RandSSID) {
  std::string ssid = RandSSID();
  
  EXPECT_FALSE(ssid.empty());
  EXPECT_EQ(ssid.substr(0, 7), "DIRECT-");
  EXPECT_EQ(ssid.length(), 32);  // "DIRECT-" (7) + 25 random chars
}

TEST(UtilsTests, RandSSIDUnique) {
  std::string ssid1 = RandSSID();
  std::string ssid2 = RandSSID();
  
  EXPECT_NE(ssid1, ssid2);
}

TEST(UtilsTests, RandWPAPassphrase) {
  std::string passphrase = RandWPAPassphrase();
  
  EXPECT_FALSE(passphrase.empty());
  EXPECT_EQ(passphrase.length(), 63);
}

TEST(UtilsTests, RandWPAPassphraseUnique) {
  std::string passphrase1 = RandWPAPassphrase();
  std::string passphrase2 = RandWPAPassphrase();
  
  EXPECT_NE(passphrase1, passphrase2);
}

}  // namespace
}  // namespace linux
}  // namespace nearby
