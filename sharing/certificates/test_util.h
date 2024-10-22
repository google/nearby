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

#ifndef LOCATION_NEARBY_CPP_SHARING_IMPLEMENTATION_CERTIFICATES_TEST_UTIL_H_
#define LOCATION_NEARBY_CPP_SHARING_IMPLEMENTATION_CERTIFICATES_TEST_UTIL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "absl/time/time.h"
#include "internal/crypto_cros/ec_private_key.h"
#include "internal/crypto_cros/symmetric_key.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/encrypted_metadata.pb.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

extern const char kTestMetadataFullName[];
extern const char kTestMetadataIconUrl[];
extern const char kTestMetadataAccountName[];

// Test Bluetooth MAC address in the format "XX:XX:XX:XX:XX:XX".
extern const char kTestUnparsedBluetoothMacAddress[];

std::unique_ptr<crypto::ECPrivateKey> GetNearbyShareTestP256KeyPair();
const std::vector<uint8_t>& GetNearbyShareTestP256PublicKey();

std::unique_ptr<crypto::SymmetricKey> GetNearbyShareTestSecretKey();
const std::vector<uint8_t>& GetNearbyShareTestCertificateId();

const std::vector<uint8_t>& GetNearbyShareTestMetadataEncryptionKey();
const std::vector<uint8_t>& GetNearbyShareTestMetadataEncryptionKeyTag();
const std::vector<uint8_t>& GetNearbyShareTestSalt();
const NearbyShareEncryptedMetadataKey& GetNearbyShareTestEncryptedMetadataKey();

absl::Time GetNearbyShareTestNotBefore();
absl::Duration GetNearbyShareTestValidityOffset();

const nearby::sharing::proto::EncryptedMetadata& GetNearbyShareTestMetadata();
const std::vector<uint8_t>& GetNearbyShareTestEncryptedMetadata();

const std::vector<uint8_t>& GetNearbyShareTestPayloadToSign();
const std::vector<uint8_t>& GetNearbyShareTestSampleSignature();
const std::vector<uint8_t>& GetNearbyShareTestPayloadHashUsingSecretKey();

NearbySharePrivateCertificate GetNearbyShareTestPrivateCertificate(
    proto::DeviceVisibility visibility,
    absl::Time not_before = GetNearbyShareTestNotBefore());
nearby::sharing::proto::PublicCertificate GetNearbyShareTestPublicCertificate(
    proto::DeviceVisibility visibility,
    absl::Time not_before = GetNearbyShareTestNotBefore());

// Returns a list of |kNearbyShareNumPrivateCertificates| private/public
// certificates, spanning contiguous validity periods.
std::vector<NearbySharePrivateCertificate>
GetNearbyShareTestPrivateCertificateList(proto::DeviceVisibility visibility);
std::vector<nearby::sharing::proto::PublicCertificate>
GetNearbyShareTestPublicCertificateList(proto::DeviceVisibility visibility);

const NearbyShareDecryptedPublicCertificate&
GetNearbyShareTestDecryptedPublicCertificate();

}  // namespace sharing
}  // namespace nearby

#endif  // LOCATION_NEARBY_CPP_SHARING_IMPLEMENTATION_CERTIFICATES_TEST_UTIL_H_
