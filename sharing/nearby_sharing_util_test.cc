// Copyright 2025 Google LLC
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

#include "sharing/nearby_sharing_util.h"

#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/proto/encrypted_metadata.pb.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby::sharing {

TEST(GetDeviceIdTest, CertificateAvailable_NoMacAddress_HasCertificateId) {
  const std::string kEndpointId = "endpoint-123";

  // Create metadata without Bluetooth MAC address
  nearby::sharing::proto::EncryptedMetadata metadata;
  metadata.set_device_name("TestDevice");

  // Create private certificate
  NearbySharePrivateCertificate private_cert(
      nearby::sharing::proto::DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE,
      absl::Now(), metadata);

  // Generate EncryptedMetadataKey
  std::optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
      private_cert.EncryptMetadataKey();
  ASSERT_TRUE(encrypted_metadata_key.has_value());

  // Generate PublicCertificate
  std::optional<nearby::sharing::proto::PublicCertificate> public_cert =
      private_cert.ToPublicCertificate();
  ASSERT_TRUE(public_cert.has_value());

  // Decrypt PublicCertificate
  std::optional<NearbyShareDecryptedPublicCertificate> certificate =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          *public_cert, *encrypted_metadata_key);

  ASSERT_TRUE(certificate.has_value());

  // Calculate expected Device ID from certificate ID
  std::string expected_device_id = absl::BytesToHexString(
      absl::string_view(reinterpret_cast<const char*>(certificate->id().data()),
                        certificate->id().size()));

  EXPECT_EQ(GetDeviceId(kEndpointId, certificate), expected_device_id);
}

}  // namespace nearby::sharing
