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
#include "presence/implementation/advertisement_decoder.h"
#include "presence/implementation/advertisement_decoder_rust_impl.h"

namespace nearby {
namespace presence {
namespace {

using ::nearby::ByteArray;                   // NOLINT
using ::nearby::internal::IdentityType;      // NOLINT
using ::nearby::internal::SharedCredential;  // NOLINT
using ::testing::ElementsAre;
using ::testing::status::StatusIs;

TEST(AdvertisementDecoderImpl, DecodePublicAdvertisement) {
  std::string V0AdvPlaintextBytes =
      "00"     // Adv Header V0 unencrypted
      "1503";  // length 1 Tx Power DE value 3
  AdvertisementDecoderImpl decoder = AdvertisementDecoderImpl();

  absl::StatusOr<Advertisement> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes(V0AdvPlaintextBytes));
  ASSERT_OK(result);
  EXPECT_EQ(result->identity_type, IdentityType::IDENTITY_TYPE_PUBLIC);
  EXPECT_EQ(result->version, 0);
  EXPECT_THAT(result->data_elements,
              ElementsAre(DataElement(DataElement::kTxPowerFieldType,
                                      absl::HexStringToBytes("03"))));
}

TEST(AdvertisementDecoderImpl, DecodePublicAdvertisementMultiDe) {
  std::string V0AdvPlaintextMultiDeBytes =
      "00"       // Adv Header V0 unencrypted
      "1505"     // length 1 Tx Power DE value 5
      "260040";  // length 2 actions de with NearbyShare bit set

  AdvertisementDecoderImpl decoder = AdvertisementDecoderImpl();
  absl::StatusOr<Advertisement> result = decoder.DecodeAdvertisement(
      absl::HexStringToBytes(V0AdvPlaintextMultiDeBytes));
  ASSERT_OK(result);
  EXPECT_EQ(result->identity_type, IdentityType::IDENTITY_TYPE_PUBLIC);
  EXPECT_EQ(result->version, 0);
  EXPECT_THAT(result->data_elements,
              ElementsAre(DataElement(DataElement::kTxPowerFieldType,
                                      absl::HexStringToBytes("05")),
                          DataElement(ActionBit::kNearbyShareAction)));
}

// V0 encrypted advertisement data - ripped out of np_adv/tests/examples_v0.rs
TEST(AdvertisementDecoderImpl, DecodeEncryptedAdvertisement) {
  std::string V0AdvEncryptedBytes = "042222D82212EF16DBF872F2A3A7C0FA5248EC";
  ByteArray seed({
      0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
      0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
      0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
  });
  ByteArray known_mac({0x09, 0xFE, 0x9E, 0x81, 0xB7, 0x3E, 0x5E, 0xCC,
                       0x76, 0x59, 0x57, 0x71, 0xE0, 0x1F, 0xFB, 0x34,
                       0x38, 0xE7, 0x5F, 0x24, 0xA7, 0x69, 0x56, 0xA0,
                       0xB8, 0xEA, 0x67, 0xD1, 0x1C, 0x3E, 0x36, 0xFD});
  SharedCredential public_credential;
  public_credential.set_key_seed(seed.AsStringView());
  public_credential.set_metadata_encryption_key_tag_v0(
      known_mac.AsStringView());
  public_credential.set_id(12345678);
  absl::flat_hash_map<IdentityType, std::vector<internal::SharedCredential>>
      credentials;
  credentials[IdentityType::IDENTITY_TYPE_PRIVATE_GROUP].push_back(
      public_credential);
  AdvertisementDecoderImpl decoder = AdvertisementDecoderImpl(&credentials);

  absl::StatusOr<Advertisement> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes(V0AdvEncryptedBytes));
  ASSERT_OK(result);
  EXPECT_EQ(result->public_credential.value().id(), public_credential.id());
  EXPECT_EQ(result->public_credential.value().key_seed(),
            public_credential.key_seed());
  EXPECT_EQ(result->identity_type, IdentityType::IDENTITY_TYPE_PRIVATE_GROUP);
  EXPECT_EQ(result->version, 0);
  EXPECT_THAT(result->data_elements,
              ElementsAre(DataElement(DataElement::kSaltFieldType,
                                      absl::HexStringToBytes("2222")),
                          DataElement(DataElement::kTxPowerFieldType,
                                      absl::HexStringToBytes("03"))));
}

TEST(AdvertisementDecoderImpl, DecodeEncryptedAdvertisementNoCreds) {
  std::string V0AdvEncryptedBytes = "042222D82212EF16DBF872F2A3A7C0FA5248EC";
  AdvertisementDecoderImpl decoder = AdvertisementDecoderImpl();

  absl::StatusOr<Advertisement> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes(V0AdvEncryptedBytes));
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kUnavailable));
}

TEST(AdvertisementDecoderImpl, V1AdvCurrentlyUnsupported) {
  std::string V1Adv =
      "20"     // Version header V1
      "00"     // format
      "02"     // section len
      "1503";  // Tx power value 3

  AdvertisementDecoderImpl decoder = AdvertisementDecoderImpl();
  absl::StatusOr<Advertisement> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes(V1Adv));
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kUnimplemented));
}

TEST(AdvertisementDecoderImpl, V0InvalidEmptyAdv) {
  std::string V1Adv = "00";
  AdvertisementDecoderImpl decoder = AdvertisementDecoderImpl();
  absl::StatusOr<Advertisement> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes(V1Adv));
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(AdvertisementDecoderImpl, V0InvalidAdvContents) {
  std::string invalid_v0_adv =
      "00"     // Adv Header V0 unencrypted
      "3503";  // length 3 Tx Power DE with only 1 byte
  AdvertisementDecoderImpl decoder = AdvertisementDecoderImpl();
  absl::StatusOr<Advertisement> result =
      decoder.DecodeAdvertisement(absl::HexStringToBytes(invalid_v0_adv));
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace presence
}  // namespace nearby
