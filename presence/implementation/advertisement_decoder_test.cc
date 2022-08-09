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
              (absl::string_view metadata_key, absl::string_view salt,
               absl::string_view data_elements),
              (override));
};

TEST(AdvertisementDecoder, DecodeBaseNpPrivateAdvertisement) {
  const std::string salt = "AB";
  const std::string metadata =
      absl::HexStringToBytes("1011121314151617181920212223");
  MockCredentialManager credential_manager;
  EXPECT_CALL(
      credential_manager,
      DecryptDataElements(metadata, salt, absl::HexStringToBytes("5051525354")))
      .WillOnce(Return(absl::HexStringToBytes("18CD29EEFF")));

  AdvertisementDecoder decoder(&credential_manager);

  const absl::StatusOr<std::vector<DataElement>> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes(
          "00204142e110111213141516171819202122235051525354"));

  ASSERT_OK(result);
  EXPECT_THAT(
      *result,
      ElementsAre(DataElement(DataElement::kSaltFieldType, salt),
                  DataElement(DataElement::kPrivateIdentityFieldType, metadata),
                  DataElement(8, absl::HexStringToBytes("CD")),
                  DataElement(9, absl::HexStringToBytes("EEFF"))));
}

TEST(AdvertisementDecoder, DecodeBaseNpPublicAdvertisement) {
  const std::string salt = "AB";
  MockCredentialManager credential_manager;
  AdvertisementDecoder decoder(&credential_manager);

  const absl::StatusOr<std::vector<DataElement>> result =
      decoder.DecodeAdvertisement(
          absl::HexStringToBytes("002041420318CD29EEFF"));

  ASSERT_OK(result);
  EXPECT_THAT(
      *result,
      ElementsAre(DataElement(DataElement::kSaltFieldType, salt),
                  DataElement(DataElement::kPublicIdentityFieldType, ""),
                  DataElement(8, absl::HexStringToBytes("CD")),
                  DataElement(9, absl::HexStringToBytes("EEFF"))));
}

TEST(AdvertisementDecoder, DecodeBaseNpPWithActionField) {
  std::string salt = "AB";
  MockCredentialManager credential_manager;
  AdvertisementDecoder decoder(&credential_manager);

  auto result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes("002041420326B840"));

  EXPECT_OK(result);
  EXPECT_THAT(*result,
              UnorderedElementsAre(
                  DataElement(DataElement::kSaltFieldType, salt),
                  DataElement(DataElement::kPublicIdentityFieldType, ""),
                  DataElement(DataElement::kContextTimestampFieldType,
                              absl::HexStringToBytes("0B")),
                  DataElement(DataElement::kActionFieldType,
                              action::kTapToTransferAction),
                  DataElement(DataElement::kActionFieldType,
                              action::kNearbyShareAction)));
}

TEST(AdvertisementDecoder, InvalidAdvertisementFieldTooShort) {
  MockCredentialManager credential_manager;
  AdvertisementDecoder decoder(&credential_manager);

  // 0x50 header means 5 bytes long salt but only 4 bytes follow.
  EXPECT_THAT(
      decoder.DecodeAdvertisement(absl::HexStringToBytes("0050A0A1A2A3")),
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
  MockCredentialManager credential_manager;
  // 0x28CD is an invalid DE, 0x28 means a 2 byte payload with type 8 alas only
  // one byte is given (0xCD)
  EXPECT_CALL(
      credential_manager,
      DecryptDataElements(metadata, salt, absl::HexStringToBytes("5051525354")))
      .WillOnce(Return(absl::HexStringToBytes("28CD")));
  AdvertisementDecoder decoder(&credential_manager);

  EXPECT_THAT(decoder.DecodeAdvertisement(absl::HexStringToBytes(
                  "00204142e110111213141516171819202122235051525354")),
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
