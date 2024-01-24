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

#ifndef THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_DECRYPTED_PUBLIC_CERTIFICATE_H_
#define THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_DECRYPTED_PUBLIC_CERTIFICATE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/crypto_cros/symmetric_key.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/proto/encrypted_metadata.pb.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

// Stores decrypted metadata and crypto keys for the remote device that uploaded
// this certificate to the Nearby Share server. Use DecryptPublicCertificate()
// to generate an instance. This class provides a method for verifying a signed
// payload during the authentication flow.
class NearbyShareDecryptedPublicCertificate {
 public:
  // Attempts to decrypt the encrypted metadata of the PublicCertificate proto
  // by first decrypting the |encrypted_metadata_key| using the secret key
  // then using the decrypted key to decrypt the metadata. Returns absl::nullopt
  // if the metadata was not successfully decrypted or if the proto data is
  // invalid.
  static std::optional<NearbyShareDecryptedPublicCertificate>
  DecryptPublicCertificate(
      const nearby::sharing::proto::PublicCertificate& public_certificate,
      const NearbyShareEncryptedMetadataKey& encrypted_metadata_key);

  NearbyShareDecryptedPublicCertificate(
      const NearbyShareDecryptedPublicCertificate& other);
  NearbyShareDecryptedPublicCertificate& operator=(
      const NearbyShareDecryptedPublicCertificate& other);
  NearbyShareDecryptedPublicCertificate(
      NearbyShareDecryptedPublicCertificate&&);
  NearbyShareDecryptedPublicCertificate& operator=(
      NearbyShareDecryptedPublicCertificate&&);

  virtual ~NearbyShareDecryptedPublicCertificate();

  const std::vector<uint8_t>& id() const { return id_; }
  absl::Time not_before() const { return not_before_; }
  absl::Time not_after() const { return not_after_; }
  const nearby::sharing::proto::EncryptedMetadata& unencrypted_metadata()
      const {
    return unencrypted_metadata_;
  }

  bool for_self_share() const { return for_self_share_; }

  // Verifies the |signature| of the signed |payload| using |public_key_|.
  // Returns true if verification was successful.
  bool VerifySignature(absl::Span<const uint8_t> payload,
                       absl::Span<const uint8_t> signature) const;

  // Creates a hash of the |authentication_token|, using |secret_key_|. The use
  // of HKDF and the output vector size is part of the Nearby Share protocol and
  // conforms with the GmsCore implementation.
  std::vector<uint8_t> HashAuthenticationToken(
      absl::Span<const uint8_t> authentication_token) const;

 private:
  NearbyShareDecryptedPublicCertificate(
      absl::Time not_before, absl::Time not_after,
      std::unique_ptr<crypto::SymmetricKey> secret_key,
      std::vector<uint8_t> public_key, std::vector<uint8_t> id,
      nearby::sharing::proto::EncryptedMetadata unencrypted_metadata,
      bool for_self_share);

  // The start and end times of the certificate's validity period. To avoid
  // issues with clock skew, these times may be offset compared to the
  // corresponding private certificate.
  absl::Time not_before_;
  absl::Time not_after_;

  // A 32-byte AES key that was used for metadata key and metadata decryption.
  // Also, used to generate an authentication token hash.
  std::unique_ptr<crypto::SymmetricKey> secret_key_;

  // A P-256 public key used for verification. The bytes comprise a DER-encoded
  // ASN.1 SubjectPublicKeyInfo from the X.509 specification (RFC 5280).
  std::vector<uint8_t> public_key_;

  // An ID for the certificate, most likely generated from the secret key.
  std::vector<uint8_t> id_;

  // Unencrypted device metadata. The proto name is misleading; it holds data
  // that was previously serialized and encrypted.
  nearby::sharing::proto::EncryptedMetadata unencrypted_metadata_;

  // Indicates if this public certificate is from another device owned by the
  // same user.
  bool for_self_share_ = false;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_DECRYPTED_PUBLIC_CERTIFICATE_H_
