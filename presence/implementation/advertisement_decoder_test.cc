// Copyright 2022 Google LLC
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

#include "presence/implementation/advertisement_decoder.h"

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
#include "presence/scan_request.h"
#include "presence/scan_request_builder.h"

namespace nearby {
namespace presence {

namespace {
using ::nearby::ByteArray;  // NOLINT
using ::nearby::internal::IdentityType;
using ::nearby::internal::SharedCredential;  // NOLINT
using ::testing::ElementsAre;
using ::protobuf_matchers::EqualsProto;
using ::testing::Matcher;
using ::testing::Pointwise;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::status::StatusIs;

constexpr absl::string_view kAccountName = "test account";

ScanRequest GetScanRequest() {
  return {.account_name = std::string(kAccountName),
          .identity_types = {IdentityType::IDENTITY_TYPE_PRIVATE,
                             IdentityType::IDENTITY_TYPE_TRUSTED,
                             IdentityType::IDENTITY_TYPE_PUBLIC,
                             IdentityType::IDENTITY_TYPE_PROVISIONED}};
}

#if USE_RUST_LDT == 1
ScanRequest GetScanRequest(std::vector<SharedCredential> credentials) {
  LegacyPresenceScanFilter scan_filter = {.remote_public_credentials =
                                              credentials};
  return ScanRequestBuilder()
      .SetAccountName(kAccountName)
      .AddIdentityType(IdentityType::IDENTITY_TYPE_PRIVATE)
      .AddIdentityType(IdentityType::IDENTITY_TYPE_TRUSTED)
      .AddIdentityType(IdentityType::IDENTITY_TYPE_PUBLIC)
      .AddIdentityType(IdentityType::IDENTITY_TYPE_PROVISIONED)
      .AddScanFilter(scan_filter)
      .Build();
}

SharedCredential GetPublicCredential() {
  // Values copied from LDT tests
  ByteArray seed({204, 219, 36, 137, 233, 252, 172, 66, 179, 147, 72,
                  184, 148, 30, 209, 154, 29,  54,  14, 117, 224, 152,
                  200, 193, 94, 107, 28,  194, 182, 32, 205, 57});
  ByteArray known_mac({223, 185, 10,  31,  155, 31, 226, 141, 24,  187, 204,
                       165, 34,  64,  181, 204, 44, 203, 95,  141, 82,  137,
                       163, 203, 100, 235, 53,  65, 202, 97,  75,  180});
  SharedCredential public_credential;
  public_credential.set_key_seed(seed.AsStringView());
  public_credential.set_metadata_encryption_key_tag(known_mac.AsStringView());
  return public_credential;
}

TEST(AdvertisementDecoder, DecodeBaseNpPrivateAdvertisement) {
  std::string salt = "AB";
  ByteArray metadata_key(
      {205, 104, 63, 225, 161, 209, 248, 70, 84, 61, 10, 19, 212, 174});
  absl::flat_hash_map<IdentityType, std::vector<internal::SharedCredential>>
      credentials;
  credentials[IdentityType::IDENTITY_TYPE_PRIVATE].push_back(
      GetPublicCredential());
  AdvertisementDecoder decoder(GetScanRequest(), &credentials);

  absl::StatusOr<Advertisement> result = decoder.DecodeAdvertisement(
      absl::HexStringToBytes("00414142ceb073b0e34f58d7dc6dea370783ac943fa5"));

  ASSERT_OK(result);
  EXPECT_EQ(result->metadata_key, metadata_key.AsStringView());
  EXPECT_EQ(result->identity_type, IdentityType::IDENTITY_TYPE_PRIVATE);
  EXPECT_THAT(result->data_elements,
              ElementsAre(DataElement(DataElement::kSaltFieldType, salt),
                          DataElement(DataElement::kTxPowerFieldType,
                                      absl::HexStringToBytes("05")),
                          DataElement(DataElement::kActionFieldType,
                                      absl::HexStringToBytes("08"))));
}

TEST(AdvertisementDecoder,
     DecodeBaseNpPrivateAdvertisementWithPublicCredentialFromScanRequest) {
  const std::string salt = "AB";
  ByteArray metadata_key(
      {205, 104, 63, 225, 161, 209, 248, 70, 84, 61, 10, 19, 212, 174});
  std::vector<SharedCredential> credentials = {GetPublicCredential()};

  AdvertisementDecoder decoder(GetScanRequest(credentials));

  absl::StatusOr<Advertisement> result = decoder.DecodeAdvertisement(
      absl::HexStringToBytes("00414142ceb073b0e34f58d7dc6dea370783ac943fa5"));

  ASSERT_OK(result);
  EXPECT_EQ(result->metadata_key, metadata_key.AsStringView());
  EXPECT_EQ(result->identity_type, IdentityType::IDENTITY_TYPE_PRIVATE);
  EXPECT_THAT(result->data_elements,
              ElementsAre(DataElement(DataElement::kSaltFieldType, salt),
                          DataElement(DataElement::kTxPowerFieldType,
                                      absl::HexStringToBytes("05")),
                          DataElement(DataElement::kActionFieldType,
                                      absl::HexStringToBytes("08"))));
}

TEST(AdvertisementDecoder, DecodeBaseNpTrustedAdvertisement) {
  std::string salt = "AB";
  ByteArray metadata_key(
      {205, 104, 63, 225, 161, 209, 248, 70, 84, 61, 10, 19, 212, 174});
  absl::flat_hash_map<IdentityType, std::vector<internal::SharedCredential>>
      credentials;
  credentials[IdentityType::IDENTITY_TYPE_TRUSTED].push_back(
      GetPublicCredential());
  AdvertisementDecoder decoder(GetScanRequest(), &credentials);

  absl::StatusOr<Advertisement> result = decoder.DecodeAdvertisement(
      absl::HexStringToBytes("00424142253536ac63191a96894d95f0ffa38b57cf9b"));

  ASSERT_OK(result);
  EXPECT_EQ(result->metadata_key, metadata_key.AsStringView());
  EXPECT_EQ(result->identity_type, IdentityType::IDENTITY_TYPE_TRUSTED);
  EXPECT_THAT(
      result->data_elements,
      UnorderedElementsAre(DataElement(DataElement::kSaltFieldType, salt),
                           DataElement(DataElement::kTxPowerFieldType,
                                       absl::HexStringToBytes("05")),
                           DataElement(DataElement::kActionFieldType,
                                       absl::HexStringToBytes("08")),
                           DataElement(DataElement::kActionFieldType,
                                       absl::HexStringToBytes("0A"))));
}

TEST(AdvertisementDecoder, DecodeBaseNpProvisionedAdvertisement) {
  std::string salt = "AB";
  ByteArray metadata_key(
      {205, 104, 63, 225, 161, 209, 248, 70, 84, 61, 10, 19, 212, 174});
  absl::flat_hash_map<IdentityType, std::vector<internal::SharedCredential>>
      credentials;
  credentials[IdentityType::IDENTITY_TYPE_PROVISIONED].push_back(
      GetPublicCredential());
  AdvertisementDecoder decoder(GetScanRequest(), &credentials);

  absl::StatusOr<Advertisement> result = decoder.DecodeAdvertisement(
      absl::HexStringToBytes("00444142253536ac63191a96894d95f0ffa38b57cf9b"));

  ASSERT_OK(result);
  EXPECT_EQ(result->metadata_key, metadata_key.AsStringView());
  EXPECT_EQ(result->identity_type, IdentityType::IDENTITY_TYPE_PROVISIONED);
  EXPECT_THAT(
      result->data_elements,
      UnorderedElementsAre(DataElement(DataElement::kSaltFieldType, salt),
                           DataElement(DataElement::kTxPowerFieldType,
                                       absl::HexStringToBytes("05")),
                           DataElement(DataElement::kActionFieldType,
                                       absl::HexStringToBytes("08")),
                           DataElement(DataElement::kActionFieldType,
                                       absl::HexStringToBytes("0A"))));
}

TEST(AdvertisementDecoder, InvalidEncryptedContent) {
  std::string salt = "AB";
  ByteArray metadata_key(
      {205, 104, 63, 225, 161, 209, 248, 70, 84, 61, 10, 19, 212, 174});
  absl::flat_hash_map<IdentityType, std::vector<internal::SharedCredential>>
      credentials;
  credentials[IdentityType::IDENTITY_TYPE_PRIVATE].push_back(
      GetPublicCredential());
  AdvertisementDecoder decoder(GetScanRequest(), &credentials);

  EXPECT_THAT(decoder.DecodeAdvertisement(absl::HexStringToBytes(
                  "00414142f085d661ac8cb110e792e7faeb736294")),
              StatusIs(absl::StatusCode::kOutOfRange));
}

#endif /*USE_RUST_LDT*/

TEST(AdvertisementDecoder, DecodeBaseNpPublicAdvertisement) {
  const std::string salt = "AB";
  AdvertisementDecoder decoder(GetScanRequest());

  const absl::StatusOr<Advertisement> result = decoder.DecodeAdvertisement(
      absl::HexStringToBytes("002041420337C1C2C31BEE"));

  ASSERT_OK(result);
  EXPECT_EQ(result->identity_type, IdentityType::IDENTITY_TYPE_PUBLIC);
  EXPECT_EQ(result->version, 0);
  EXPECT_THAT(
      result->data_elements,
      ElementsAre(DataElement(DataElement::kSaltFieldType, salt),
                  DataElement(DataElement::kPublicIdentityFieldType, ""),
                  DataElement(DataElement::kModelIdFieldType,
                              absl::HexStringToBytes("C1C2C3")),
                  DataElement(DataElement::kBatteryFieldType,
                              absl::HexStringToBytes("EE"))));
}

TEST(AdvertisementDecoder, DecodeBaseNpWithTxActionField) {
  std::string salt = "AB";
  AdvertisementDecoder decoder(GetScanRequest());

  auto result = decoder.DecodeAdvertisement(
      absl::HexStringToBytes("00204142034650B04180"));

  EXPECT_OK(result);
  EXPECT_THAT(result->data_elements,
              UnorderedElementsAre(
                  DataElement(DataElement::kSaltFieldType, salt),
                  DataElement(DataElement::kPublicIdentityFieldType, ""),
                  DataElement(DataElement::kTxPowerFieldType,
                              absl::HexStringToBytes("50")),
                  DataElement(DataElement::kContextTimestampFieldType,
                              absl::HexStringToBytes("0B")),
                  DataElement(DataElement(ActionBit::kEddystoneAction)),
                  DataElement(DataElement(ActionBit::kTapToTransferAction)),
                  DataElement(DataElement(ActionBit::kNearbyShareAction))));
}

TEST(AdvertisementDecoder,
     ScanForEncryptedIdentityIgnoresPublicIdentityAdvertisement) {
  AdvertisementDecoder decoder(
      {.account_name = std::string(kAccountName),
       .identity_types = {IdentityType::IDENTITY_TYPE_PRIVATE,
                          IdentityType::IDENTITY_TYPE_TRUSTED,
                          IdentityType::IDENTITY_TYPE_PROVISIONED}});

  EXPECT_THAT(decoder.DecodeAdvertisement(
                  absl::HexStringToBytes("00204142034650B04180")),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(AdvertisementDecoder, DecodeEddystone) {
  AdvertisementDecoder decoder(GetScanRequest());
  std::string eddystone_id =
      absl::HexStringToBytes("A0A1A2A3A4A5A6A7A8A9B0B1B2B3B4B5B6B7B8B9");

  auto result = decoder.DecodeAdvertisement(absl::HexStringToBytes("0008") +
                                            eddystone_id);

  EXPECT_OK(result);
  EXPECT_THAT(result->data_elements,
              ElementsAre(DataElement(DataElement::kEddystoneIdFieldType,
                                      eddystone_id)));
}

// TODO(b/238214467): Add more negative tests
TEST(AdvertisementDecoder, UnsupportedDataElement) {
  std::string valid_header_and_salt = absl::HexStringToBytes("00204142");
  AdvertisementDecoder decoder(GetScanRequest());

  EXPECT_THAT(decoder.DecodeAdvertisement(valid_header_and_salt +
                                          absl::HexStringToBytes("0D")),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(AdvertisementDecoder, InvalidAdvertisementFieldTooShort) {
  AdvertisementDecoder decoder(GetScanRequest());

  // 0x59 header means 5 bytes long Account Key Data but only 4 bytes follow.
  EXPECT_THAT(
      decoder.DecodeAdvertisement(absl::HexStringToBytes("0059A0A1A2A3")),
      StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(AdvertisementDecoder, ZeroLengthPayload) {
  AdvertisementDecoder decoder(GetScanRequest());

  // A action with type 0xA and no payload
  const absl::StatusOr<Advertisement> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes("000A"));

  ASSERT_OK(result);
  EXPECT_THAT(result->data_elements, ElementsAre(DataElement(0xA, "")));
}

TEST(AdvertisementDecoder, EmptyAdvertisement) {
  AdvertisementDecoder decoder(GetScanRequest());

  EXPECT_THAT(decoder.DecodeAdvertisement(""),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(AdvertisementDecoder, UnsupportedAdvertisementVersion) {
  AdvertisementDecoder decoder(GetScanRequest());

  EXPECT_THAT(decoder.DecodeAdvertisement(
                  absl::HexStringToBytes("012041420318CD29EEFF")),
              StatusIs(absl::StatusCode::kUnimplemented));
}

TEST(AdvertisementDecoder, MatchesScanFilterNoFilterPasses) {
  std::vector<DataElement> adv = {
      DataElement(DataElement::kPrivateIdentityFieldType, "payload")};
  ScanRequest empty_scan_request = {};
  AdvertisementDecoder decoder(empty_scan_request);

  // A scan request without scan filters matches any advertisement
  EXPECT_TRUE(decoder.MatchesScanFilter(
      {DataElement(DataElement::kPrivateIdentityFieldType, "payload")}));
  EXPECT_TRUE(decoder.MatchesScanFilter({}));
}

TEST(AdvertisementDecoder, MatchesPresenceScanFilter) {
  std::vector<DataElement> adv = {
      DataElement(DataElement::kPrivateIdentityFieldType, "payload")};
  DataElement model_id =
      DataElement(DataElement::kModelIdFieldType, "model id");
  DataElement salt = DataElement(DataElement::kSaltFieldType, "salt");
  DataElement salt2 = DataElement(DataElement::kSaltFieldType, "salt 2");
  PresenceScanFilter filter = {.extended_properties = {model_id, salt}};

  AdvertisementDecoder decoder(

      ScanRequestBuilder().AddScanFilter(filter).Build());

  EXPECT_FALSE(decoder.MatchesScanFilter({}));
  EXPECT_FALSE(decoder.MatchesScanFilter({salt}));
  EXPECT_TRUE(decoder.MatchesScanFilter({salt, model_id}));
  EXPECT_TRUE(decoder.MatchesScanFilter({salt, salt2, model_id}));
  EXPECT_FALSE(decoder.MatchesScanFilter({salt2, model_id}));
}

TEST(AdvertisementDecoder, MatchesLegacyPresenceScanFilter) {
  std::vector<DataElement> adv = {
      DataElement(DataElement::kPrivateIdentityFieldType, "payload")};
  DataElement model_id =
      DataElement(DataElement::kModelIdFieldType, "model id");
  DataElement salt = DataElement(DataElement::kSaltFieldType, "salt");
  DataElement salt2 = DataElement(DataElement::kSaltFieldType, "salt 2");
  LegacyPresenceScanFilter filter = {.extended_properties = {model_id, salt}};

  AdvertisementDecoder decoder(

      ScanRequestBuilder().AddScanFilter(filter).Build());

  EXPECT_FALSE(decoder.MatchesScanFilter({}));
  EXPECT_FALSE(decoder.MatchesScanFilter({salt}));
  EXPECT_TRUE(decoder.MatchesScanFilter({salt, model_id}));
  EXPECT_TRUE(decoder.MatchesScanFilter({salt, salt2, model_id}));
  EXPECT_FALSE(decoder.MatchesScanFilter({salt2, model_id}));
}

TEST(AdvertisementDecoder, MatchesLegacyPresenceScanFilterWithActions) {
  std::vector<DataElement> adv = {
      DataElement(DataElement::kPrivateIdentityFieldType, "payload")};
  DataElement model_id =
      DataElement(DataElement::kModelIdFieldType, "model id");
  DataElement salt = DataElement(DataElement::kSaltFieldType, "salt");
  DataElement eddystone_action = DataElement(ActionBit::kEddystoneAction);
  LegacyPresenceScanFilter filter = {
      .actions = {static_cast<int>(ActionBit::kActiveUnlockAction),
                  static_cast<int>(ActionBit::kEddystoneAction)},
      .extended_properties = {model_id, salt}};

  AdvertisementDecoder decoder(

      ScanRequestBuilder().AddScanFilter(filter).Build());

  EXPECT_FALSE(decoder.MatchesScanFilter({salt, model_id}));
  EXPECT_TRUE(decoder.MatchesScanFilter({salt, eddystone_action, model_id}));
}

TEST(AdvertisementDecoder, MatchesMultipleFilters) {
  std::vector<DataElement> adv = {
      DataElement(DataElement::kPrivateIdentityFieldType, "payload")};
  DataElement model_id =
      DataElement(DataElement::kModelIdFieldType, "model id");
  DataElement salt = DataElement(DataElement::kSaltFieldType, "salt");
  DataElement eddystone_action = DataElement(ActionBit::kEddystoneAction);
  PresenceScanFilter presence_filter = {.extended_properties = {model_id}};
  LegacyPresenceScanFilter legacy_filter = {
      .actions = {static_cast<int>(ActionBit::kActiveUnlockAction),
                  static_cast<int>(ActionBit::kEddystoneAction)},
      .extended_properties = {salt}};

  AdvertisementDecoder decoder(ScanRequestBuilder()
                                   .AddScanFilter(presence_filter)
                                   .AddScanFilter(legacy_filter)
                                   .Build());

  EXPECT_TRUE(decoder.MatchesScanFilter({model_id}));
  EXPECT_TRUE(decoder.MatchesScanFilter({salt, eddystone_action}));
  EXPECT_FALSE(decoder.MatchesScanFilter({eddystone_action}));
}

}  // namespace
}  // namespace presence
}  // namespace nearby
