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

#include "sharing/certificates/nearby_share_private_certificate.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <queue>
#include <vector>

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "sharing/certificates/constants.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/certificates/test_util.h"
#include "sharing/internal/api/private_certificate_data.h"
#include "sharing/proto/encrypted_metadata.pb.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

using ::nearby::sharing::proto::DeviceVisibility;

TEST(NearbySharePrivateCertificateTest, Construction) {
  NearbySharePrivateCertificate private_certificate(
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
      GetNearbyShareTestNotBefore(), GetNearbyShareTestMetadata());
  EXPECT_EQ(kNearbyShareNumBytesCertificateId, private_certificate.id().size());
  EXPECT_EQ(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
            private_certificate.visibility());
  EXPECT_EQ(GetNearbyShareTestNotBefore(), private_certificate.not_before());
  EXPECT_EQ(
      GetNearbyShareTestNotBefore() + kNearbyShareCertificateValidityPeriod,
      private_certificate.not_after());
  EXPECT_EQ(GetNearbyShareTestMetadata().SerializeAsString(),
            private_certificate.unencrypted_metadata().SerializeAsString());
}

TEST(NearbySharePrivateCertificateTest, ToFromDictionary) {
  NearbySharePrivateCertificate before(
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
      GetNearbyShareTestNotBefore(), GetNearbyShareTestMetadata());
  // Generate a few consumed salts.
  for (size_t i = 0; i < 5; ++i) ASSERT_TRUE(before.EncryptMetadataKey());

  NearbySharePrivateCertificate after(
      *NearbySharePrivateCertificate::FromCertificateData(
          before.ToCertificateData()));

  EXPECT_EQ(before.id(), after.id());
  EXPECT_EQ(before.visibility(), after.visibility());
  EXPECT_EQ(before.not_before(), after.not_before());
  EXPECT_EQ(before.not_after(), after.not_after());
  EXPECT_EQ(before.unencrypted_metadata().SerializeAsString(),
            after.unencrypted_metadata().SerializeAsString());
  EXPECT_EQ(before.secret_key_->key(), after.secret_key_->key());
  EXPECT_EQ(before.metadata_encryption_key_, after.metadata_encryption_key_);
  EXPECT_EQ(before.consumed_salts_, after.consumed_salts_);

  std::vector<uint8_t> before_private_key, after_private_key;
  before.key_pair_->ExportPrivateKey(&before_private_key);
  after.key_pair_->ExportPrivateKey(&after_private_key);
  EXPECT_EQ(before_private_key, after_private_key);
}

TEST(NearbySharePrivateCertificateTest,
     ConsumedSaltsSerializationOrderAndRoundtrip) {
  NearbySharePrivateCertificate before(
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
      GetNearbyShareTestNotBefore(), GetNearbyShareTestMetadata());
  const std::vector<std::vector<uint8_t>> consumed_salts = {
      {0xff, 0xff}, {0x01, 0x00}, {0x00, 0x01}, {0x00, 0x00}};
  for (const std::vector<uint8_t>& salt : consumed_salts) {
    before.next_salts_for_testing().push(salt);
    std::optional<NearbyShareEncryptedMetadataKey> key =
        before.EncryptMetadataKey();
    ASSERT_TRUE(key);
    EXPECT_EQ(key->salt(), salt);
  }

  api::PrivateCertificateData cert_data = before.ToCertificateData();
  EXPECT_EQ(cert_data.consumed_salts, "000000010100FFFF");

  std::optional<NearbySharePrivateCertificate> after =
      NearbySharePrivateCertificate::FromCertificateData(cert_data);
  ASSERT_TRUE(after);
  for (const std::vector<uint8_t>& salt : consumed_salts) {
    after->next_salts_for_testing().push(salt);
  }
  const std::vector<uint8_t> unused_salt = {0x12, 0x34};
  after->next_salts_for_testing().push(unused_salt);
  std::optional<NearbyShareEncryptedMetadataKey> next_key =
      after->EncryptMetadataKey();
  ASSERT_TRUE(next_key);
  EXPECT_EQ(next_key->salt(), unused_salt);
}

TEST(NearbySharePrivateCertificateTest,
     FromCertificateData_CorruptedConsumedSalts) {
  NearbySharePrivateCertificate cert(
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
      GetNearbyShareTestNotBefore(), GetNearbyShareTestMetadata());
  api::PrivateCertificateData cert_data = cert.ToCertificateData();

  cert_data.consumed_salts = "000";
  EXPECT_FALSE(NearbySharePrivateCertificate::FromCertificateData(cert_data));

  cert_data.consumed_salts = "0000F";
  EXPECT_FALSE(NearbySharePrivateCertificate::FromCertificateData(cert_data));

  cert_data.consumed_salts = "ZZZZ";
  EXPECT_FALSE(NearbySharePrivateCertificate::FromCertificateData(cert_data));
}

TEST(NearbySharePrivateCertificateTest, EncryptMetadataKey) {
  NearbySharePrivateCertificate private_certificate(
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
      GetNearbyShareTestNotBefore(), GetNearbyShareTestMetadata());
  std::optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
      private_certificate.EncryptMetadataKey();
  ASSERT_TRUE(encrypted_metadata_key);
  EXPECT_EQ(kNearbyShareNumBytesMetadataEncryptionKeySalt,
            encrypted_metadata_key->salt().size());
  EXPECT_EQ(kNearbyShareNumBytesMetadataEncryptionKey,
            encrypted_metadata_key->encrypted_key().size());
}

TEST(NearbySharePrivateCertificateTest, EncryptMetadataKey_FixedData) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  std::optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
      private_certificate.EncryptMetadataKey();
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().encrypted_key(),
            encrypted_metadata_key->encrypted_key());
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().salt(),
            encrypted_metadata_key->salt());
}

TEST(NearbySharePrivateCertificateTest,
     EncryptMetadataKey_SaltsExhaustedFailure) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  for (size_t i = 0; i < kNearbyShareMaxNumMetadataEncryptionKeySalts; ++i) {
    EXPECT_TRUE(private_certificate.EncryptMetadataKey());
  }
  EXPECT_FALSE(private_certificate.EncryptMetadataKey());
}

TEST(NearbySharePrivateCertificateTest,
     EncryptMetadataKey_TooManySaltGenerationRetriesFailure) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  EXPECT_TRUE(private_certificate.EncryptMetadataKey());
  while (private_certificate.next_salts_for_testing().size() <
         kNearbyShareMaxNumMetadataEncryptionKeySaltGenerationRetries) {
    private_certificate.next_salts_for_testing().push(GetNearbyShareTestSalt());
  }
  EXPECT_FALSE(private_certificate.EncryptMetadataKey());
}

TEST(NearbySharePrivateCertificateTest, PublicCertificateConversion) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS);
  private_certificate.offset_for_testing() = GetNearbyShareTestValidityOffset();
  std::optional<nearby::sharing::proto::PublicCertificate> public_certificate =
      private_certificate.ToPublicCertificate();
  ASSERT_TRUE(public_certificate);
  EXPECT_EQ(GetNearbyShareTestPublicCertificate(
                DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS)
                .SerializeAsString(),
            public_certificate->SerializeAsString());
}

TEST(NearbySharePrivateCertificateTest, EncryptDecryptRoundtrip) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);

  std::optional<NearbyShareDecryptedPublicCertificate>
      decrypted_public_certificate =
          NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
              *private_certificate.ToPublicCertificate(),
              *private_certificate.EncryptMetadataKey());
  ASSERT_TRUE(decrypted_public_certificate);
  EXPECT_EQ(
      private_certificate.unencrypted_metadata().SerializeAsString(),
      decrypted_public_certificate->unencrypted_metadata().SerializeAsString());
}

TEST(NearbySharePrivateCertificateTest, SignVerifyRoundtrip) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  std::optional<std::vector<uint8_t>> signature =
      private_certificate.Sign(GetNearbyShareTestPayloadToSign());
  ASSERT_TRUE(signature);

  std::optional<NearbyShareDecryptedPublicCertificate>
      decrypted_public_certificate =
          NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
              *private_certificate.ToPublicCertificate(),
              *private_certificate.EncryptMetadataKey());
  EXPECT_TRUE(decrypted_public_certificate->VerifySignature(
      GetNearbyShareTestPayloadToSign(), *signature));
}

TEST(NearbySharePrivateCertificateTest, HashAuthenticationToken) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  EXPECT_EQ(GetNearbyShareTestPayloadHashUsingSecretKey(),
            private_certificate.HashAuthenticationToken(
                GetNearbyShareTestPayloadToSign()));
}

}  // namespace sharing
}  // namespace nearby
