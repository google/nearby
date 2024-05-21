// Copyright 2024 Google LLC
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

#include "sharing/advertisement.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "sharing/common/nearby_share_enums.h"

namespace nearby {
namespace sharing {
namespace {

struct TestParameters {
  std::vector<uint8_t> salt;
  std::vector<uint8_t> encrypted_metadata_key;
  ShareTargetType target_type;
  std::optional<std::string> target_name;
  int vendor_id;
};

class AdvertisementTest : public testing::TestWithParam<TestParameters> {};

TEST_P(AdvertisementTest, TestAdvertisementRoundTrip) {
  auto params = GetParam();
  auto advertisement = Advertisement::NewInstance(
      params.salt, params.encrypted_metadata_key, params.target_type,
      params.target_name, params.vendor_id);
  auto bytes = advertisement->ToEndpointInfo();
  auto advertisement_from_bytes = Advertisement::FromEndpointInfo(bytes);
  EXPECT_EQ(*advertisement_from_bytes, *advertisement);
}

INSTANTIATE_TEST_SUITE_P(
    ShareTargetTypes, AdvertisementTest,
    testing::Values(
        TestParameters{.salt = std::vector<uint8_t>(Advertisement::kSaltSize),
                       .encrypted_metadata_key = std::vector<uint8_t>(
                           Advertisement::kMetadataEncryptionKeyHashByteSize),
                       .target_type = ShareTargetType::kLaptop,
                       .target_name = std::nullopt,
                       .vendor_id = 0},
        TestParameters{.salt = std::vector<uint8_t>(Advertisement::kSaltSize),
                       .encrypted_metadata_key = std::vector<uint8_t>(
                           Advertisement::kMetadataEncryptionKeyHashByteSize),
                       .target_type = ShareTargetType::kPhone,
                       .target_name = std::nullopt,
                       .vendor_id = 0},
        TestParameters{.salt = std::vector<uint8_t>(Advertisement::kSaltSize),
                       .encrypted_metadata_key = std::vector<uint8_t>(
                           Advertisement::kMetadataEncryptionKeyHashByteSize),
                       .target_type = ShareTargetType::kTablet,
                       .target_name = std::nullopt,
                       .vendor_id = 0}));
INSTANTIATE_TEST_SUITE_P(
    VendorIds, AdvertisementTest,
    testing::Values(
        TestParameters{.salt = std::vector<uint8_t>(Advertisement::kSaltSize),
                       .encrypted_metadata_key = std::vector<uint8_t>(
                           Advertisement::kMetadataEncryptionKeyHashByteSize),
                       .target_type = ShareTargetType::kPhone,
                       .target_name = std::nullopt,
                       .vendor_id = 0},
        TestParameters{.salt = std::vector<uint8_t>(Advertisement::kSaltSize),
                       .encrypted_metadata_key = std::vector<uint8_t>(
                           Advertisement::kMetadataEncryptionKeyHashByteSize),
                       .target_type = ShareTargetType::kPhone,
                       .target_name = std::nullopt,
                       .vendor_id = 1}));
INSTANTIATE_TEST_SUITE_P(
    TargetNames, AdvertisementTest,
    testing::Values(
        TestParameters{.salt = std::vector<uint8_t>(Advertisement::kSaltSize),
                       .encrypted_metadata_key = std::vector<uint8_t>(
                           Advertisement::kMetadataEncryptionKeyHashByteSize),
                       .target_type = ShareTargetType::kPhone,
                       .target_name = std::nullopt,
                       .vendor_id = 0},
        TestParameters{.salt = std::vector<uint8_t>(Advertisement::kSaltSize),
                       .encrypted_metadata_key = std::vector<uint8_t>(
                           Advertisement::kMetadataEncryptionKeyHashByteSize),
                       .target_type = ShareTargetType::kPhone,
                       .target_name = "Test device",
                       .vendor_id = 0}));

INSTANTIATE_TEST_SUITE_P(
    SaltAndKeyCombo, AdvertisementTest,
    testing::Values(
        TestParameters{.salt = std::vector<uint8_t>(Advertisement::kSaltSize),
                       .encrypted_metadata_key = std::vector<uint8_t>(
                           Advertisement::kMetadataEncryptionKeyHashByteSize),
                       .target_type = ShareTargetType::kLaptop,
                       .target_name = std::nullopt,
                       .vendor_id = 0},
        TestParameters{
            .salt = std::vector<uint8_t>(Advertisement::kSaltSize, 255),
            .encrypted_metadata_key = std::vector<uint8_t>(
                Advertisement::kMetadataEncryptionKeyHashByteSize, 255),
            .target_type = ShareTargetType::kLaptop,
            .target_name = std::nullopt,
            .vendor_id = 0}));

}  // namespace
}  // namespace sharing
}  // namespace nearby
