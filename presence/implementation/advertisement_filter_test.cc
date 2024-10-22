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

#include "presence/implementation/advertisement_filter.h"

#include <array>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/proto/credential.pb.h"
#include "presence/data_element.h"
#include "presence/implementation/advertisement_decoder.h"
#include "presence/scan_request.h"
#include "presence/scan_request_builder.h"

namespace nearby {
namespace presence {
namespace {

TEST(AdvertisementFilter, MatchesScanFilterNoFilterPasses) {
  std::vector<DataElement> adv = {
      DataElement(DataElement::kPrivateGroupIdentityFieldType, "payload")};
  ScanRequest empty_scan_request = {};
  AdvertisementFilter adv_filter(empty_scan_request);

  // A scan request without scan filters matches any advertisement
  EXPECT_TRUE(adv_filter.MatchesScanFilter(
      {.data_elements = {DataElement(
           DataElement::kPrivateGroupIdentityFieldType, "payload")}}));
  EXPECT_TRUE(adv_filter.MatchesScanFilter({}));
}

TEST(AdvertisementFilter, MatchesPresenceScanFilter) {
  std::vector<DataElement> adv = {
      DataElement(DataElement::kPrivateGroupIdentityFieldType, "payload")};
  DataElement model_id =
      DataElement(DataElement::kModelIdFieldType, "model id");
  DataElement salt = DataElement(DataElement::kSaltFieldType, "salt");
  DataElement salt2 = DataElement(DataElement::kSaltFieldType, "salt 2");
  PresenceScanFilter filter = {.extended_properties = {model_id, salt}};

  AdvertisementFilter adv_filter(
      ScanRequestBuilder().AddScanFilter(filter).Build());

  EXPECT_FALSE(adv_filter.MatchesScanFilter({}));
  EXPECT_FALSE(adv_filter.MatchesScanFilter({.data_elements = {salt}}));
  EXPECT_TRUE(
      adv_filter.MatchesScanFilter({.data_elements = {salt, model_id}}));
  EXPECT_TRUE(
      adv_filter.MatchesScanFilter({.data_elements = {salt, salt2, model_id}}));
  EXPECT_FALSE(
      adv_filter.MatchesScanFilter({.data_elements = {salt2, model_id}}));
}

TEST(AdvertisementFilter, MatchesLegacyPresenceScanFilter) {
  std::vector<DataElement> adv = {
      DataElement(DataElement::kPrivateGroupIdentityFieldType, "payload")};
  DataElement model_id =
      DataElement(DataElement::kModelIdFieldType, "model id");
  DataElement salt = DataElement(DataElement::kSaltFieldType, "salt");
  DataElement salt2 = DataElement(DataElement::kSaltFieldType, "salt 2");
  LegacyPresenceScanFilter filter = {.extended_properties = {model_id, salt}};

  AdvertisementFilter adv_filter(
      ScanRequestBuilder().AddScanFilter(filter).Build());

  EXPECT_FALSE(adv_filter.MatchesScanFilter(Advertisement{}));
  EXPECT_FALSE(adv_filter.MatchesScanFilter({.data_elements = {salt}}));
  EXPECT_TRUE(
      adv_filter.MatchesScanFilter({.data_elements = {salt, model_id}}));
  EXPECT_TRUE(
      adv_filter.MatchesScanFilter({.data_elements = {salt, salt2, model_id}}));
  EXPECT_FALSE(adv_filter.MatchesScanFilter(
      Advertisement{.data_elements = {salt2, model_id}}));
}

TEST(AdvertisementFilter,
     EncryptedIdentityFilterIgnoresPublicIdentityAdvertisement) {
  AdvertisementFilter adv_filter(
      {.identity_types = {
           internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP,
           internal::IdentityType::IDENTITY_TYPE_CONTACTS_GROUP}});

  EXPECT_FALSE(adv_filter.MatchesScanFilter(
      {.identity_type = internal::IdentityType::IDENTITY_TYPE_PUBLIC}));
  EXPECT_TRUE(adv_filter.MatchesScanFilter(
      {.identity_type = internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP}));
}

TEST(AdvertisementFilter, PublicIdentityFilterMatchesPublicIdentityAdv) {
  AdvertisementFilter adv_filter(
      {.identity_types = {internal::IdentityType::IDENTITY_TYPE_PUBLIC}});

  EXPECT_TRUE(adv_filter.MatchesScanFilter(
      {.identity_type = internal::IdentityType::IDENTITY_TYPE_PUBLIC}));
  EXPECT_FALSE(adv_filter.MatchesScanFilter(
      {.identity_type = internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP}));
}

TEST(AdvertisementFilter, EmptyIdentityFilterMatchesAllAdvIdentityTypes) {
  AdvertisementFilter adv_filter({});

  EXPECT_TRUE(adv_filter.MatchesScanFilter(
      {.identity_type = internal::IdentityType::IDENTITY_TYPE_PUBLIC}));
  EXPECT_TRUE(adv_filter.MatchesScanFilter(
      {.identity_type = internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP}));
}

TEST(AdvertisementFilter, MatchesLegacyPresenceScanFilterWithActions) {
  std::vector<DataElement> adv = {
      DataElement(DataElement::kPrivateGroupIdentityFieldType, "payload")};
  DataElement model_id =
      DataElement(DataElement::kModelIdFieldType, "model id");
  DataElement salt = DataElement(DataElement::kSaltFieldType, "salt");
  DataElement ttt_action = DataElement(ActionBit::kTapToTransferAction);
  LegacyPresenceScanFilter filter = {
      .actions = {static_cast<int>(ActionBit::kActiveUnlockAction),
                  static_cast<int>(ActionBit::kTapToTransferAction)},
      .extended_properties = {model_id, salt}};

  AdvertisementFilter adv_filter(
      ScanRequestBuilder().AddScanFilter(filter).Build());

  EXPECT_FALSE(
      adv_filter.MatchesScanFilter({.data_elements = {salt, model_id}}));
  EXPECT_TRUE(adv_filter.MatchesScanFilter(
      {.data_elements = {salt, ttt_action, model_id}}));
}

TEST(AdvertisementFilter, MatchesMultipleFilters) {
  std::vector<DataElement> adv = {
      DataElement(DataElement::kPrivateGroupIdentityFieldType, "payload")};
  DataElement model_id =
      DataElement(DataElement::kModelIdFieldType, "model id");
  DataElement salt = DataElement(DataElement::kSaltFieldType, "salt");
  DataElement ttt_action = DataElement(ActionBit::kTapToTransferAction);
  PresenceScanFilter presence_filter = {.extended_properties = {model_id}};
  LegacyPresenceScanFilter legacy_filter = {
      .actions = {static_cast<int>(ActionBit::kActiveUnlockAction),
                  static_cast<int>(ActionBit::kTapToTransferAction)},
      .extended_properties = {salt}};

  AdvertisementFilter adv_filter(ScanRequestBuilder()
                                     .AddScanFilter(presence_filter)
                                     .AddScanFilter(legacy_filter)
                                     .Build());

  EXPECT_TRUE(adv_filter.MatchesScanFilter({.data_elements = {model_id}}));
  EXPECT_TRUE(
      adv_filter.MatchesScanFilter({.data_elements = {salt, ttt_action}}));
  EXPECT_FALSE(adv_filter.MatchesScanFilter({.data_elements = {ttt_action}}));
}

}  // namespace
}  // namespace presence
}  // namespace nearby
