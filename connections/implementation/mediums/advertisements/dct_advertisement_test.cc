// Copyright 2025 Google LLC
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

#include "connections/implementation/mediums/advertisements/dct_advertisement.h"

#include <optional>
#include <string>

#include "gtest/gtest.h"

namespace nearby::connections::advertisements::ble {
namespace {

TEST(DctAdvertisementTest, Create) {
  std::optional<DctAdvertisement> advertisement =
      DctAdvertisement::Create("service_id", "device", 0x1234);
  ASSERT_TRUE(advertisement.has_value());
  EXPECT_EQ(advertisement->GetVersion(), DctAdvertisement::kVersion);
  EXPECT_EQ(advertisement->GetPsm(), 0x1234);
  EXPECT_EQ(advertisement->GetServiceIdHash(), "\x96\x77");
  EXPECT_EQ(advertisement->GetDeviceName(), "device");
}

TEST(DctAdvertisementTest, CreateWithInvalidParameters) {
  EXPECT_FALSE(DctAdvertisement::Create("service_id", "", 0x1234).has_value());
  EXPECT_FALSE(DctAdvertisement::Create("", "device", 0x1234).has_value());
  EXPECT_FALSE(DctAdvertisement::Create("service_id", "device", 0).has_value());
}

TEST(DctAdvertisementTest, ParseWithInvalidParameters) {
  std::string data = "\x70\01";
  EXPECT_FALSE(DctAdvertisement::Parse(data).has_value());
}

TEST(DctAdvertisementTest, CreateWithTruncatedDeviceName) {
  std::optional<DctAdvertisement> advertisement = DctAdvertisement::Create(
      "service_id", "\xC3\xA9\xC3\xB1\xC3\xB6\xF0\x9F\x98\x80", 0x1234);
  EXPECT_EQ(advertisement->GetDeviceName(), "\xC3\xA9\xC3\xB1\xC3\xB6");
  advertisement = DctAdvertisement::Create(
      "service_id", "\xF0\x9F\x98\x80\xC3\xA9\xC3\xB1\xC3\xB6", 0x1234);
  EXPECT_EQ(advertisement->GetDeviceName(), "\xF0\x9F\x98\x80\xC3\xA9");
  advertisement = DctAdvertisement::Create("service_id", "abcdefghi", 0x1234);
  EXPECT_EQ(advertisement->GetDeviceName(), "abcdefg");
  advertisement = DctAdvertisement::Create(
      "service_id", "\xF0\x9F\x98\x80\xF0\x9F\xAA\xB4", 0x1234);
  EXPECT_EQ(advertisement->GetDeviceName(), "\xF0\x9F\x98\x80");
  advertisement = DctAdvertisement::Create(
      "service_id", "\xC3\xA9\xC3\xB1\xC3\xB6\xF0\x9F\x98", 0x1234);
  EXPECT_FALSE(advertisement.has_value());
}

TEST(DctAdvertisementTest, CreateAndParse) {
  std::optional<DctAdvertisement> advertisement = DctAdvertisement::Create(
      "service_id", "\xE4\xBD\xA0\xE5\xA5\xBD\xE7\x9A\x84", 0x1234);
  ASSERT_TRUE(advertisement.has_value());
  EXPECT_EQ(advertisement->GetVersion(), DctAdvertisement::kVersion);
  EXPECT_EQ(advertisement->GetPsm(), 0x1234);
  EXPECT_EQ(advertisement->GetServiceIdHash(), "\x96\x77");
  EXPECT_EQ(advertisement->GetDeviceName(), "\xE4\xBD\xA0\xE5\xA5\xBD");
  std::string data = advertisement->ToData();
  std::optional<DctAdvertisement> parsed_advertisement =
      DctAdvertisement::Parse(data);
  ASSERT_TRUE(parsed_advertisement.has_value());
  EXPECT_EQ(parsed_advertisement->GetVersion(), DctAdvertisement::kVersion);
  EXPECT_EQ(parsed_advertisement->GetPsm(), 0x1234);
  EXPECT_EQ(parsed_advertisement->GetServiceIdHash(), "\x96\x77");
  EXPECT_EQ(parsed_advertisement->GetDeviceName(), "\xE4\xBD\xA0\xE5\xA5\xBD");
}

}  // namespace
}  // namespace nearby::connections::advertisements::ble
