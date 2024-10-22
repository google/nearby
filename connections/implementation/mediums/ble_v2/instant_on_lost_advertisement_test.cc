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

#include "connections/implementation/mediums/ble_v2/instant_on_lost_advertisement.h"

#include <array>
#include <list>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {

constexpr std::array<const std::string, 3> kGoodHash = {
    "\x01\x02\x03\x04", "\x05\x06\x07\x08", "\x09\x0A\x0B\x0C"};

constexpr std::array<const std::string, 3> kBadHash = {
    "\x01\x02\x03\x04", "x06\x07\x08", "\x09\x0A\x0B\x0C"};

using ::testing::status::StatusIs;

TEST(InstantOnLostAdvertisementTest,
     InstantOnLostAdvertisementParsesFromGoodHash) {
  absl::StatusOr<InstantOnLostAdvertisement> advertisement =
      InstantOnLostAdvertisement::CreateFromHashes(
          std::list<std::string>(kGoodHash.begin(), kGoodHash.end()));
  ASSERT_OK(advertisement);
  EXPECT_EQ(advertisement->ToBytes().size(), 14);

  absl::StatusOr<InstantOnLostAdvertisement> des_advertisement =
      InstantOnLostAdvertisement::CreateFromBytes(advertisement->ToBytes());
  ASSERT_OK(des_advertisement);
  EXPECT_EQ(des_advertisement->hashes(),
            std::list<std::string>(kGoodHash.begin(), kGoodHash.end()));
}

TEST(InstantOnLostAdvertisementTest,
     InstantOnLostAdvertisementFailsParseFromBadHash) {
  absl::StatusOr<InstantOnLostAdvertisement> advertisement =
      InstantOnLostAdvertisement::CreateFromHashes(
          std::list<std::string>(kBadHash.begin(), kBadHash.end()));
  EXPECT_THAT(advertisement, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(InstantOnLostAdvertisementTest,
     InstantOnLostAdvertisementFailsParseFromLessBytes) {
  EXPECT_THAT(InstantOnLostAdvertisement::CreateFromBytes("01"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(InstantOnLostAdvertisement::CreateFromBytes("test"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(InstantOnLostAdvertisementTest,
     InstantOnLostAdvertisementFailsParseBadVersion) {
  absl::StatusOr<InstantOnLostAdvertisement> advertisement =
      InstantOnLostAdvertisement::CreateFromHashes(
          std::list<std::string>(kGoodHash.begin(), kGoodHash.end()));
  ASSERT_OK(advertisement);
  // Set version to 0.
  std::string advertisement_bytes = advertisement->ToBytes();
  advertisement_bytes[0] = 0x10;

  EXPECT_THAT(InstantOnLostAdvertisement::CreateFromBytes(advertisement_bytes),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(InstantOnLostAdvertisementTest,
     InstantOnLostAdvertisementFailsParseBadAdvertisementType) {
  absl::StatusOr<InstantOnLostAdvertisement> advertisement =
      InstantOnLostAdvertisement::CreateFromHashes(
          std::list<std::string>(kGoodHash.begin(), kGoodHash.end()));
  ASSERT_OK(advertisement);
  // Set advertisement type to 0.
  std::string advertisement_bytes = advertisement->ToBytes();
  advertisement_bytes[0] = 0x01;

  EXPECT_THAT(InstantOnLostAdvertisement::CreateFromBytes(advertisement_bytes),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
