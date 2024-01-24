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

#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/types/span.h"
#include "sharing/certificates/common.h"
#include "sharing/certificates/constants.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/certificates/test_util.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/encrypted_metadata.pb.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/proto/timestamp.pb.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::proto::DeviceVisibility;
using ::nearby::sharing::proto::PublicCertificate;

// The for_selected_contacts field of a public certificate proto is irrelevant
// for remote device certificates. Even if it is set, it is meaningless. It only
// has meaning for private certificates converted to public certificates and
// uploaded to the Nearby server.
const DeviceVisibility kTestPublicCertificateVisibility =
    DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE;

TEST(NearbyShareDecryptedPublicCertificateTest, Decrypt) {
  PublicCertificate proto_cert =
      GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility);
  proto_cert.set_for_self_share(true);

  std::optional<NearbyShareDecryptedPublicCertificate> cert =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          proto_cert, GetNearbyShareTestEncryptedMetadataKey());
  EXPECT_TRUE(cert);
  EXPECT_EQ(FromJavaTime(proto_cert.start_time().seconds() * 1000),
            cert->not_before());
  EXPECT_EQ(FromJavaTime(proto_cert.end_time().seconds() * 1000),
            cert->not_after());
  EXPECT_EQ(std::vector<uint8_t>(proto_cert.secret_id().begin(),
                                 proto_cert.secret_id().end()),
            cert->id());
  EXPECT_EQ(GetNearbyShareTestMetadata().SerializeAsString(),
            cert->unencrypted_metadata().SerializeAsString());
  EXPECT_EQ(proto_cert.for_self_share(), cert->for_self_share());
}

TEST(NearbyShareDecryptedPublicCertificateTest, Decrypt_IncorrectKeyFailure) {
  // Input incorrect metadata encryption key.
  EXPECT_FALSE(NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
      GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility),
      NearbyShareEncryptedMetadataKey(
          std::vector<uint8_t>(kNearbyShareNumBytesMetadataEncryptionKeySalt,
                               0x00),
          std::vector<uint8_t>(kNearbyShareNumBytesMetadataEncryptionKey,
                               0x00))));
}

TEST(NearbyShareDecryptedPublicCertificateTest,
     Decrypt_MetadataDecryptionFailure) {
  // Use metadata that cannot be decrypted with the given key.
  PublicCertificate proto_cert =
      GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility);
  proto_cert.set_encrypted_metadata_bytes("invalid metadata");
  EXPECT_FALSE(NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
      proto_cert, GetNearbyShareTestEncryptedMetadataKey()));
}

TEST(NearbyShareDecryptedPublicCertificateTest, Decrypt_InvalidDataFailure) {
  // Do not accept the input PublicCertificate because the validity period does
  // not make sense.
  PublicCertificate proto_cert =
      GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility);
  proto_cert.mutable_end_time()->set_seconds(proto_cert.start_time().seconds() -
                                             1);
  EXPECT_FALSE(NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
      proto_cert, GetNearbyShareTestEncryptedMetadataKey()));
}

TEST(NearbyShareDecryptedPublicCertificateTest, Verify) {
  std::optional<NearbyShareDecryptedPublicCertificate> cert =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility),
          GetNearbyShareTestEncryptedMetadataKey());
  EXPECT_TRUE(cert->VerifySignature(GetNearbyShareTestPayloadToSign(),
                                    GetNearbyShareTestSampleSignature()));
}

TEST(NearbyShareDecryptedPublicCertificateTest, Verify_InitFailure) {
  // Public key has invalid SubjectPublicKeyInfo format.
  PublicCertificate proto_cert =
      GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility);
  proto_cert.set_public_key("invalid public key");

  auto cert = NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
      proto_cert, GetNearbyShareTestEncryptedMetadataKey());
  ASSERT_TRUE(cert);
  EXPECT_FALSE(cert->VerifySignature(GetNearbyShareTestPayloadToSign(),
                                     GetNearbyShareTestSampleSignature()));
}

TEST(NearbyShareDecryptedPublicCertificateTest, Verify_WrongSignature) {
  auto cert = NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
      GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility),
      GetNearbyShareTestEncryptedMetadataKey());
  EXPECT_FALSE(
      cert->VerifySignature(GetNearbyShareTestPayloadToSign(),
                            /*signature=*/absl::Span<const uint8_t>()));
}

TEST(NearbyShareDecryptedPublicCertificateTest, HashAuthenticationToken) {
  auto cert = NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
      GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility),
      GetNearbyShareTestEncryptedMetadataKey());
  EXPECT_EQ(GetNearbyShareTestPayloadHashUsingSecretKey(),
            cert->HashAuthenticationToken(GetNearbyShareTestPayloadToSign()));
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
