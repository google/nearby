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

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/proto/credential.pb.h"
#include "presence/data_element.h"
#include "presence/implementation/advertisement_decoder_impl.h"
#include "presence/scan_request.h"
#include "presence/scan_request_builder.h"

namespace nearby {
namespace presence {

namespace {
using ::nearby::ByteArray;                   // NOLINT
using ::nearby::internal::IdentityType;      // NOLINT
using ::nearby::internal::SharedCredential;  // NOLINT
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;
using ::testing::status::StatusIs;

constexpr absl::string_view kAccountName = "test account";

ScanRequest GetScanRequest() {
  return {.account_name = std::string(kAccountName),
          .identity_types = {
              IdentityType::IDENTITY_TYPE_PRIVATE_GROUP,
              IdentityType::IDENTITY_TYPE_CONTACTS_GROUP,
              IdentityType::IDENTITY_TYPE_PUBLIC,
          }};
}

ScanRequest GetScanRequest(std::vector<SharedCredential> credentials) {
  LegacyPresenceScanFilter scan_filter = {.remote_public_credentials =
                                              credentials};
  return ScanRequestBuilder()
      .SetAccountName(kAccountName)
      .AddIdentityType(IdentityType::IDENTITY_TYPE_PRIVATE_GROUP)
      .AddIdentityType(IdentityType::IDENTITY_TYPE_CONTACTS_GROUP)
      .AddIdentityType(IdentityType::IDENTITY_TYPE_PUBLIC)
      .Build();
}

SharedCredential GetPublicCredential() {
  // Values copied from LDT tests
  ByteArray seed({204, 219, 36, 137, 233, 252, 172, 66, 179, 147, 72,
                  184, 148, 30, 209, 154, 29,  54,  14, 117, 224, 152,
                  200, 193, 94, 107, 28,  194, 182, 32, 205, 57});
  ByteArray known_mac({0xB4, 0xC5, 0x9F, 0xA5, 0x99, 0x24, 0x1B, 0x81,
                       0x75, 0x8D, 0x97, 0x6B, 0x5A, 0x62, 0x1C, 0x05,
                       0x23, 0x2F, 0xE1, 0xBF, 0x89, 0xAE, 0x59, 0x87,
                       0xCA, 0x25, 0x4C, 0x35, 0x54, 0xDC, 0xE5, 0x0E});
  SharedCredential public_credential;
  public_credential.set_key_seed(seed.AsStringView());
  public_credential.set_metadata_encryption_key_tag_v0(
      known_mac.AsStringView());
  return public_credential;
}

TEST(AdvertisementDecoderImpl,
     DecodeBaseNpV0PublicIdentityWithTxAndActionFields) {
  AdvertisementDecoderImpl decoder;
  // v0 public identity, power and action, action value 8 for active unlock.
  // These values all come from
  // //third_party/nearby/presence/implementation/advertisement_factory_test.cc
  auto result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes("000315FF260080"));

  ASSERT_OK(result);
  EXPECT_THAT(result->data_elements,
              UnorderedElementsAre(
                  DataElement(DataElement::kPublicIdentityFieldType, ""),
                  DataElement(DataElement::kTxPowerFieldType,
                              absl::HexStringToBytes("ff")),
                  DataElement(DataElement(ActionBit::kActiveUnlockAction))));
}

TEST(AdvertisementDecoderImpl, DecodeBaseNpPublicAdvertisement) {
  const std::string salt = "AB";
  AdvertisementDecoderImpl decoder;

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

TEST(AdvertisementDecoderImpl, DecodeBaseNpWithTxAndActionFields) {
  std::string salt = "AB";
  AdvertisementDecoderImpl decoder;

  auto result = decoder.DecodeAdvertisement(
      absl::HexStringToBytes("0020414203155036B04180"));

  ASSERT_OK(result);
  EXPECT_THAT(result->data_elements,
              UnorderedElementsAre(
                  DataElement(DataElement::kSaltFieldType, salt),
                  DataElement(DataElement::kPublicIdentityFieldType, ""),
                  DataElement(DataElement::kTxPowerFieldType,
                              absl::HexStringToBytes("50")),
                  DataElement(DataElement::kContextTimestampFieldType,
                              absl::HexStringToBytes("0B")),
                  DataElement(DataElement(ActionBit::kTapToTransferAction)),
                  DataElement(DataElement(ActionBit::kNearbyShareAction))));
}

TEST(AdvertisementDecoderImpl, DecodeBaseNpPrivateAdvertisement) {
  std::string salt = "AB";
  ByteArray metadata_key(
      {205, 104, 63, 225, 161, 209, 248, 70, 84, 61, 10, 19, 212, 174});
  absl::flat_hash_map<IdentityType, std::vector<internal::SharedCredential>>
      credentials;
  credentials[IdentityType::IDENTITY_TYPE_PRIVATE_GROUP].push_back(
      GetPublicCredential());
  AdvertisementDecoderImpl decoder(&credentials);

  absl::StatusOr<Advertisement> result = decoder.DecodeAdvertisement(
      absl::HexStringToBytes("00514142b8412efb0bc657ba514baf4d1b50ddc842cd1c"));
  ASSERT_OK(result);
  EXPECT_EQ(result->metadata_key, metadata_key.AsStringView());
  EXPECT_EQ(result->identity_type, IdentityType::IDENTITY_TYPE_PRIVATE_GROUP);
  EXPECT_THAT(result->data_elements,
              ElementsAre(DataElement(DataElement::kSaltFieldType, salt),
                          DataElement(DataElement::kTxPowerFieldType,
                                      absl::HexStringToBytes("05")),
                          DataElement(DataElement::kActionFieldType,
                                      absl::HexStringToBytes("08"))));
}

TEST(AdvertisementDecoderImpl, InvalidEncryptedContent) {
  std::string salt = "AB";
  ByteArray metadata_key(
      {205, 104, 63, 225, 161, 209, 248, 70, 84, 61, 10, 19, 212, 174});
  absl::flat_hash_map<IdentityType, std::vector<internal::SharedCredential>>
      credentials;
  credentials[IdentityType::IDENTITY_TYPE_PRIVATE_GROUP].push_back(
      GetPublicCredential());
  AdvertisementDecoderImpl decoder(&credentials);

  EXPECT_THAT(decoder.DecodeAdvertisement(absl::HexStringToBytes(
                  "00414142f085d661ac8cb110e792e7faeb736294")),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(AdvertisementDecoderImpl, UnsupportedDataElement) {
  std::string valid_header_and_salt = absl::HexStringToBytes("00204142");
  AdvertisementDecoderImpl decoder;

  EXPECT_THAT(decoder.DecodeAdvertisement(valid_header_and_salt +
                                          absl::HexStringToBytes("0D")),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(AdvertisementDecoderImpl, InvalidAdvertisementFieldTooShort) {
  AdvertisementDecoderImpl decoder;

  // 0x59 header means 5 bytes long Account Key Data but only 4 bytes follow.
  EXPECT_THAT(
      decoder.DecodeAdvertisement(absl::HexStringToBytes("0059A0A1A2A3")),
      StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(AdvertisementDecoderImpl, ZeroLengthPayload) {
  AdvertisementDecoderImpl decoder;

  // A action with type 0xA and no payload
  const absl::StatusOr<Advertisement> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes("000A"));

  ASSERT_OK(result);
  EXPECT_THAT(result->data_elements, ElementsAre(DataElement(0xA, "")));
}

TEST(AdvertisementDecoderImpl, EmptyAdvertisement) {
  AdvertisementDecoderImpl decoder;

  EXPECT_THAT(decoder.DecodeAdvertisement(""),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(AdvertisementDecoderImpl, UnsupportedAdvertisementVersion) {
  AdvertisementDecoderImpl decoder;

  EXPECT_THAT(decoder.DecodeAdvertisement(
                  absl::HexStringToBytes("012041420318CD29EEFF")),
              StatusIs(absl::StatusCode::kUnimplemented));
}

}  // namespace
}  // namespace presence
}  // namespace nearby
