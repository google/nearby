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
#include "absl/strings/escaping.h"
#include "presence/data_element.h"
#include "presence/implementation/credential_manager_impl.h"

namespace nearby {
namespace presence {

namespace {
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::status::StatusIs;

class MockCredentialManager : public CredentialManagerImpl {
 public:
  MOCK_METHOD(absl::StatusOr<std::string>, DecryptDataElements,
              (absl::string_view salt, absl::string_view data_elements),
              (override));
};

TEST(AdvertisementDecoder, DecodeBaseNpPrivateAdvertisement) {
  const std::string salt = "AB";
  const std::string metadata =
      absl::HexStringToBytes("1011121314151617181920212223");
  const std::string encrypted_metadata =
      absl::HexStringToBytes("F01112131415161718192021222F");
  MockCredentialManager credential_manager;
  EXPECT_CALL(
      credential_manager,
      DecryptDataElements(
          salt, encrypted_metadata + absl::HexStringToBytes("505152535455")))
      .WillOnce(Return(metadata + absl::HexStringToBytes("37C1C2C31BEE")));

  AdvertisementDecoder decoder(&credential_manager);

  const absl::StatusOr<std::vector<DataElement>> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes(
          "00614142F01112131415161718192021222F505152535455"));

  ASSERT_OK(result);
  EXPECT_THAT(
      *result,
      ElementsAre(DataElement(DataElement::kSaltFieldType, salt),
                  DataElement(DataElement::kPrivateIdentityFieldType, metadata),
                  DataElement(DataElement::kModelIdFieldType,
                              absl::HexStringToBytes("C1C2C3")),
                  DataElement(DataElement::kBatteryFieldType,
                              absl::HexStringToBytes("EE"))));
}

TEST(AdvertisementDecoder, DecodeBaseNpTrustedAdvertisement) {
  const std::string salt = "AB";
  const std::string metadata =
      absl::HexStringToBytes("1011121314151617181920212223");
  const std::string encrypted_metadata =
      absl::HexStringToBytes("F01112131415161718192021222F");
  MockCredentialManager credential_manager;
  EXPECT_CALL(
      credential_manager,
      DecryptDataElements(
          salt, encrypted_metadata + absl::HexStringToBytes("505152535455")))
      .WillOnce(Return(metadata + absl::HexStringToBytes("3AC1C2C31BEE")));

  AdvertisementDecoder decoder(&credential_manager);

  const absl::StatusOr<std::vector<DataElement>> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes(
          "00624142F01112131415161718192021222F505152535455"));

  ASSERT_OK(result);
  EXPECT_THAT(
      *result,
      ElementsAre(DataElement(DataElement::kSaltFieldType, salt),
                  DataElement(DataElement::kTrustedIdentityFieldType, metadata),
                  DataElement(DataElement::kConnectionStatusFieldType,
                              absl::HexStringToBytes("C1C2C3")),
                  DataElement(DataElement::kBatteryFieldType,
                              absl::HexStringToBytes("EE"))));
}

TEST(AdvertisementDecoder, DecodeBaseNpProvisionedAdvertisement) {
  const std::string salt = "AB";
  const std::string metadata =
      absl::HexStringToBytes("1011121314151617181920212223");
  const std::string encrypted_metadata =
      absl::HexStringToBytes("F01112131415161718192021222F");
  MockCredentialManager credential_manager;
  EXPECT_CALL(
      credential_manager,
      DecryptDataElements(
          salt, encrypted_metadata + absl::HexStringToBytes("505152535455")))
      .WillOnce(Return(metadata + absl::HexStringToBytes("59C1C2C3C4C5")));

  AdvertisementDecoder decoder(&credential_manager);

  const absl::StatusOr<std::vector<DataElement>> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes(
          "00644142F01112131415161718192021222F505152535455"));

  ASSERT_OK(result);
  EXPECT_THAT(
      *result,
      ElementsAre(
          DataElement(DataElement::kSaltFieldType, salt),
          DataElement(DataElement::kProvisionedIdentityFieldType, metadata),
          DataElement(DataElement::kAccountKeyDataFieldType,
                      absl::HexStringToBytes("C1C2C3C4C5"))));
}

TEST(AdvertisementDecoder, DecodeBaseNpPublicAdvertisement) {
  const std::string salt = "AB";
  MockCredentialManager credential_manager;
  AdvertisementDecoder decoder(&credential_manager);

  const absl::StatusOr<std::vector<DataElement>> result =
      decoder.DecodeAdvertisement(
          absl::HexStringToBytes("002041420337C1C2C31BEE"));

  ASSERT_OK(result);
  EXPECT_THAT(
      *result,
      ElementsAre(DataElement(DataElement::kSaltFieldType, salt),
                  DataElement(DataElement::kPublicIdentityFieldType, ""),
                  DataElement(DataElement::kModelIdFieldType,
                              absl::HexStringToBytes("C1C2C3")),
                  DataElement(DataElement::kBatteryFieldType,
                              absl::HexStringToBytes("EE"))));
}

TEST(AdvertisementDecoder, DecodeBaseNpPrivateAdvertisementWithTxActionField) {
  const std::string salt = "AB";
  const std::string metadata =
      absl::HexStringToBytes("1011121314151617181920212223");
  const std::string encrypted_metadata =
      absl::HexStringToBytes("F01112131415161718192021222F");
  MockCredentialManager credential_manager;
  EXPECT_CALL(
      credential_manager,
      DecryptDataElements(
          salt, encrypted_metadata + absl::HexStringToBytes("505152535455")))
      .WillOnce(Return(metadata + absl::HexStringToBytes("4650B04180")));

  AdvertisementDecoder decoder(&credential_manager);

  const absl::StatusOr<std::vector<DataElement>> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes(
          "00614142F01112131415161718192021222F505152535455"));

  ASSERT_OK(result);
  EXPECT_THAT(*result,
              UnorderedElementsAre(
                  DataElement(DataElement::kSaltFieldType, salt),
                  DataElement(DataElement::kPrivateIdentityFieldType, metadata),
                  DataElement(DataElement::kTxPowerFieldType,
                              absl::HexStringToBytes("50")),
                  DataElement(DataElement::kContextTimestampFieldType,
                              absl::HexStringToBytes("0B")),
                  DataElement(DataElement(ActionBit::kEddystoneAction)),
                  DataElement(DataElement(ActionBit::kTapToTransferAction)),
                  DataElement(DataElement(ActionBit::kNearbyShareAction))));
}

TEST(AdvertisementDecoder, DecodeBaseNpWithTxActionField) {
  std::string salt = "AB";
  MockCredentialManager credential_manager;
  AdvertisementDecoder decoder(&credential_manager);

  auto result = decoder.DecodeAdvertisement(
      absl::HexStringToBytes("00204142034650B04180"));

  EXPECT_OK(result);
  EXPECT_THAT(*result,
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

TEST(AdvertisementDecoder, DecodeEddystone) {
  MockCredentialManager credential_manager;
  AdvertisementDecoder decoder(&credential_manager);
  std::string eddystone_id =
      absl::HexStringToBytes("A0A1A2A3A4A5A6A7A8A9B0B1B2B3B4B5B6B7B8B9");

  auto result = decoder.DecodeAdvertisement(absl::HexStringToBytes("0008") +
                                            eddystone_id);

  EXPECT_OK(result);
  EXPECT_THAT(*result, ElementsAre(DataElement(
                           DataElement::kEddystoneIdFieldType, eddystone_id)));
}

// TODO(b/238214467): Add more negative tests
TEST(AdvertisementDecoder, UnsupportedDataElement) {
  std::string valid_header_and_salt = absl::HexStringToBytes("00204142");
  MockCredentialManager credential_manager;
  AdvertisementDecoder decoder(&credential_manager);

  EXPECT_THAT(decoder.DecodeAdvertisement(valid_header_and_salt +
                                          absl::HexStringToBytes("0D")),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(AdvertisementDecoder, InvalidAdvertisementFieldTooShort) {
  MockCredentialManager credential_manager;
  AdvertisementDecoder decoder(&credential_manager);

  // 0x59 header means 5 bytes long Account Key Data but only 4 bytes follow.
  EXPECT_THAT(
      decoder.DecodeAdvertisement(absl::HexStringToBytes("0059A0A1A2A3")),
      StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(AdvertisementDecoder, ZeroLengthPayload) {
  MockCredentialManager credential_manager;
  AdvertisementDecoder decoder(&credential_manager);

  // A action with type 0xA and no payload
  const absl::StatusOr<std::vector<DataElement>> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes("000A"));

  ASSERT_OK(result);
  EXPECT_THAT(*result, ElementsAre(DataElement(0xA, "")));
}

TEST(AdvertisementDecoder, EmptyAdvertisement) {
  MockCredentialManager credential_manager;
  AdvertisementDecoder decoder(&credential_manager);

  EXPECT_THAT(decoder.DecodeAdvertisement(""),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(AdvertisementDecoder, InvalidEncryptedContent) {
  const std::string salt = "AB";
  const std::string metadata =
      absl::HexStringToBytes("1011121314151617181920212223");
  const std::string encrypted_metadata =
      absl::HexStringToBytes("F01112131415161718192021222F");
  MockCredentialManager credential_manager;
  // 0x37CD is an invalid DE, 0x37 means a 3 byte payload with type 7 alas only
  // one byte is given (0xCD)
  EXPECT_CALL(
      credential_manager,
      DecryptDataElements(
          salt, encrypted_metadata + absl::HexStringToBytes("505152535455")))
      .WillOnce(Return(metadata + absl::HexStringToBytes("37CD")));
  AdvertisementDecoder decoder(&credential_manager);

  EXPECT_THAT(decoder.DecodeAdvertisement(absl::HexStringToBytes(
                  "00614142F01112131415161718192021222F505152535455")),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(AdvertisementDecoder, UnsupportedAdvertisementVersion) {
  MockCredentialManager credential_manager;
  AdvertisementDecoder decoder(&credential_manager);

  EXPECT_THAT(decoder.DecodeAdvertisement(
                  absl::HexStringToBytes("012041420318CD29EEFF")),
              StatusIs(absl::StatusCode::kUnimplemented));
}

}  // namespace
}  // namespace presence
}  // namespace nearby
