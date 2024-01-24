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
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/crypto_cros/aead.h"
#include "internal/crypto_cros/ec_private_key.h"
#include "internal/crypto_cros/ec_signature_creator.h"
#include "internal/crypto_cros/encryptor.h"
#include "internal/crypto_cros/hmac.h"
#include "internal/crypto_cros/sha2.h"
#include "internal/crypto_cros/symmetric_key.h"
#include "sharing/certificates/common.h"
#include "sharing/certificates/constants.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/internal/api/private_certificate_data.h"
#include "sharing/internal/base/encode.h"
#include "sharing/internal/public/logging.h"
#include "sharing/proto/encrypted_metadata.pb.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/proto/timestamp.pb.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::api::PrivateCertificateData;
using ::nearby::sharing::proto::DeviceVisibility;

// Generates a random validity bound offset in the interval
// [0, kNearbyShareMaxPrivateCertificateValidityBoundOffset).
absl::Duration GenerateRandomOffset() {
  absl::BitGen bitgen;
  return absl::Microseconds(
      absl::Uniform(bitgen, 0,
                    kNearbyShareMaxPrivateCertificateValidityBoundOffset /
                        absl::Microseconds(1)));
}

// Generates a certificate identifier by hashing the input secret |key|.
std::vector<uint8_t> CreateCertificateIdFromSecretKey(
    const crypto::SymmetricKey& key) {
  NL_DCHECK_EQ(crypto::kSHA256Length, kNearbyShareNumBytesCertificateId);
  std::vector<uint8_t> id(kNearbyShareNumBytesCertificateId);
  crypto::SHA256HashString(key.key(), id.data(), id.size());

  return id;
}

// Creates an HMAC from |metadata_encryption_key| to be used as a key commitment
// in certificates.
std::optional<std::vector<uint8_t>> CreateMetadataEncryptionKeyTag(
    absl::Span<const uint8_t> metadata_encryption_key) {
  // This array of 0x00 is used to conform with the GmsCore implementation.
  std::vector<uint8_t> key(kNearbyShareNumBytesMetadataEncryptionKeyTag, 0x00);

  std::vector<uint8_t> result(kNearbyShareNumBytesMetadataEncryptionKeyTag);
  crypto::HMAC hmac(crypto::HMAC::HashAlgorithm::SHA256);
  if (!hmac.Init(key) ||
      !hmac.Sign(metadata_encryption_key,
                 absl::MakeSpan(result.data(), result.size())))
    return std::nullopt;

  return result;
}

std::string EncodeString(absl::string_view unencoded_string) {
  std::string result;
  absl::WebSafeBase64Escape(unencoded_string, &result);
  return result;
}

std::optional<std::string> DecodeString(const std::string* encoded_string) {
  std::string result;
  if (!encoded_string) return std::nullopt;

  if (absl::WebSafeBase64Unescape(*encoded_string, &result)) {
    return result;
  }
  return std::nullopt;
}

std::string BytesToEncodedString(const std::vector<uint8_t>& bytes) {
  return EncodeString(std::string(bytes.begin(), bytes.end()));
}

std::optional<std::vector<uint8_t>> EncodedStringToBytes(
    const std::string* str) {
  std::optional<std::string> decoded_str = DecodeString(str);
  return decoded_str ? std::make_optional<std::vector<uint8_t>>(
                           decoded_str->begin(), decoded_str->end())
                     : std::nullopt;
}

std::string SaltsToString(const std::set<std::vector<uint8_t>>& salts) {
  std::string str;
  str.reserve(salts.size() * 2 * kNearbyShareNumBytesMetadataEncryptionKeySalt);
  for (const std::vector<uint8_t>& salt : salts) {
    str += nearby::utils::HexEncode(salt);
  }
  return str;
}

std::set<std::vector<uint8_t>> StringToSalts(absl::string_view str) {
  const size_t chars_per_salt =
      2 * kNearbyShareNumBytesMetadataEncryptionKeySalt;
  NL_DCHECK_EQ(str.size() % chars_per_salt, 0);
  std::set<std::vector<uint8_t>> salts;
  for (size_t i = 0; i < str.size(); i += chars_per_salt) {
    std::string bytes =
        absl::HexStringToBytes(absl::string_view(&str[i], chars_per_salt));
    std::vector<uint8_t> salt(bytes.begin(), bytes.end());
    salts.insert(std::move(salt));
  }
  return salts;
}

}  // namespace

NearbySharePrivateCertificate::NearbySharePrivateCertificate(
    DeviceVisibility visibility, absl::Time not_before,
    nearby::sharing::proto::EncryptedMetadata unencrypted_metadata)
    : visibility_(visibility),
      not_before_(not_before),
      not_after_(not_before_ + kNearbyShareCertificateValidityPeriod),
      key_pair_(crypto::ECPrivateKey::Create()),
      secret_key_(crypto::SymmetricKey::GenerateRandomKey(
          crypto::SymmetricKey::Algorithm::AES,
          /*key_size_in_bits=*/8 * kNearbyShareNumBytesSecretKey)),
      metadata_encryption_key_(
          GenerateRandomBytes(kNearbyShareNumBytesMetadataEncryptionKey)),
      id_(CreateCertificateIdFromSecretKey(*secret_key_)),
      unencrypted_metadata_(std::move(unencrypted_metadata)) {
  NL_DCHECK_NE(
      static_cast<int>(visibility),
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED));
}

NearbySharePrivateCertificate::NearbySharePrivateCertificate(
    DeviceVisibility visibility, absl::Time not_before, absl::Time not_after,
    std::unique_ptr<crypto::ECPrivateKey> key_pair,
    std::unique_ptr<crypto::SymmetricKey> secret_key,
    std::vector<uint8_t> metadata_encryption_key, std::vector<uint8_t> id,
    nearby::sharing::proto::EncryptedMetadata unencrypted_metadata,
    std::set<std::vector<uint8_t>> consumed_salts)
    : visibility_(visibility),
      not_before_(not_before),
      not_after_(not_after),
      key_pair_(std::move(key_pair)),
      secret_key_(std::move(secret_key)),
      metadata_encryption_key_(std::move(metadata_encryption_key)),
      id_(std::move(id)),
      unencrypted_metadata_(std::move(unencrypted_metadata)),
      consumed_salts_(std::move(consumed_salts)) {
  NL_DCHECK_NE(
      static_cast<int>(visibility),
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED));
}

NearbySharePrivateCertificate::NearbySharePrivateCertificate(
    const NearbySharePrivateCertificate& other) {
  *this = other;
}

NearbySharePrivateCertificate& NearbySharePrivateCertificate::operator=(
    const NearbySharePrivateCertificate& other) {
  if (this == &other) return *this;

  visibility_ = other.visibility_;
  not_before_ = other.not_before_;
  not_after_ = other.not_after_;
  key_pair_ = other.key_pair_->Copy();
  secret_key_ = crypto::SymmetricKey::Import(
      crypto::SymmetricKey::Algorithm::AES, other.secret_key_->key());
  metadata_encryption_key_ = other.metadata_encryption_key_;
  id_ = other.id_;
  unencrypted_metadata_ = other.unencrypted_metadata_;
  consumed_salts_ = other.consumed_salts_;
  next_salts_for_testing_ = other.next_salts_for_testing_;
  offset_for_testing_ = other.offset_for_testing_;
  return *this;
}

NearbySharePrivateCertificate::NearbySharePrivateCertificate(
    NearbySharePrivateCertificate&& other) = default;

NearbySharePrivateCertificate& NearbySharePrivateCertificate::operator=(
    NearbySharePrivateCertificate&& other) = default;

NearbySharePrivateCertificate::~NearbySharePrivateCertificate() = default;

std::optional<NearbyShareEncryptedMetadataKey>
NearbySharePrivateCertificate::EncryptMetadataKey() {
  std::optional<std::vector<uint8_t>> salt = GenerateUnusedSalt();
  if (!salt) {
    NL_LOG(ERROR) << "Encryption failed: Salt generation unsuccessful.";
    return std::nullopt;
  }

  std::unique_ptr<crypto::Encryptor> encryptor =
      CreateNearbyShareCtrEncryptor(secret_key_.get(), *salt);
  if (!encryptor) {
    NL_LOG(ERROR) << "Encryption failed: Could not create CTR encryptor.";
    return std::nullopt;
  }

  NL_DCHECK_EQ(kNearbyShareNumBytesMetadataEncryptionKey,
               metadata_encryption_key_.size());
  std::vector<uint8_t> encrypted_metadata_key;
  if (!encryptor->Encrypt(metadata_encryption_key_, &encrypted_metadata_key)) {
    NL_LOG(ERROR) << "Encryption failed: Could not encrypt metadata key.";
    return std::nullopt;
  }

  return NearbyShareEncryptedMetadataKey(*salt, encrypted_metadata_key);
}

std::optional<std::vector<uint8_t>> NearbySharePrivateCertificate::Sign(
    absl::Span<const uint8_t> payload) const {
  std::unique_ptr<crypto::ECSignatureCreator> signer(
      crypto::ECSignatureCreator::Create(key_pair_.get()));

  std::vector<uint8_t> signature;
  if (!signer->Sign(payload, &signature)) {
    NL_LOG(ERROR) << "Signing failed.";
    return std::nullopt;
  }

  return signature;
}

std::vector<uint8_t> NearbySharePrivateCertificate::HashAuthenticationToken(
    absl::Span<const uint8_t> authentication_token) const {
  return ComputeAuthenticationTokenHash(
      authentication_token, as_bytes(absl::MakeSpan(secret_key_->key())));
}

std::optional<nearby::sharing::proto::PublicCertificate>
NearbySharePrivateCertificate::ToPublicCertificate() const {
  std::vector<uint8_t> public_key;
  if (!key_pair_->ExportPublicKey(&public_key)) {
    NL_LOG(ERROR) << "Failed to export public key.";
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> encrypted_metadata_bytes =
      EncryptMetadata();
  if (!encrypted_metadata_bytes) {
    NL_LOG(ERROR) << "Failed to encrypt metadata.";
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> metadata_encryption_key_tag =
      CreateMetadataEncryptionKeyTag(metadata_encryption_key_);
  if (!metadata_encryption_key_tag) {
    NL_LOG(ERROR) << "Failed to compute metadata encryption key tag.";
    return std::nullopt;
  }

  absl::Duration not_before_offset =
      offset_for_testing_.value_or(GenerateRandomOffset());
  absl::Duration not_after_offset =
      offset_for_testing_.value_or(GenerateRandomOffset());

  nearby::sharing::proto::PublicCertificate public_certificate;
  public_certificate.set_secret_id(std::string(id_.begin(), id_.end()));
  public_certificate.set_secret_key(secret_key_->key());
  public_certificate.set_public_key(
      std::string(public_key.begin(), public_key.end()));
  public_certificate.mutable_start_time()->set_seconds(
      ToJavaTime(not_before_ - not_before_offset) / 1000);
  public_certificate.mutable_end_time()->set_seconds(
      ToJavaTime(not_after_ + not_after_offset) / 1000);
  public_certificate.set_for_selected_contacts(
      visibility_ == DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS);
  public_certificate.set_metadata_encryption_key(std::string(
      metadata_encryption_key_.begin(), metadata_encryption_key_.end()));
  public_certificate.set_encrypted_metadata_bytes(std::string(
      encrypted_metadata_bytes->begin(), encrypted_metadata_bytes->end()));
  public_certificate.set_metadata_encryption_key_tag(
      std::string(metadata_encryption_key_tag->begin(),
                  metadata_encryption_key_tag->end()));
  public_certificate.set_for_self_share(
      visibility_ == DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);

  return public_certificate;
}

PrivateCertificateData NearbySharePrivateCertificate::ToCertificateData()
    const {
  std::vector<uint8_t> key_pair;
  key_pair_->ExportPrivateKey(&key_pair);
  return PrivateCertificateData{
      .visibility = static_cast<int>(visibility_),
      .not_before = absl::ToUnixNanos(not_before_),
      .not_after = absl::ToUnixNanos(not_after_),
      .key_pair = BytesToEncodedString(key_pair),
      .secret_key = EncodeString(secret_key_->key()),
      .metadata_encryption_key = BytesToEncodedString(metadata_encryption_key_),
      .id = BytesToEncodedString(id_),
      .unencrypted_metadata_proto =
          EncodeString(unencrypted_metadata_.SerializeAsString()),
      .consumed_salts = SaltsToString(consumed_salts_),
  };
}

std::optional<NearbySharePrivateCertificate>
NearbySharePrivateCertificate::FromCertificateData(
    const PrivateCertificateData& cert_data) {
  auto bytes_opt = EncodedStringToBytes(&cert_data.key_pair);
  if (!bytes_opt) return std::nullopt;
  std::unique_ptr<crypto::ECPrivateKey> key_pair =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(*bytes_opt);
  auto str_opt = DecodeString(&cert_data.secret_key);
  if (!str_opt) return std::nullopt;
  std::unique_ptr<crypto::SymmetricKey> secret_key =
      crypto::SymmetricKey::Import(crypto::SymmetricKey::Algorithm::AES,
                                   *str_opt);
  bytes_opt = EncodedStringToBytes(&cert_data.metadata_encryption_key);
  if (!bytes_opt) return std::nullopt;
  std::vector<uint8_t> metadata_encryption_key = *bytes_opt;
  bytes_opt = EncodedStringToBytes(&cert_data.id);
  if (!bytes_opt) return std::nullopt;
  std::vector<uint8_t> id = *bytes_opt;
  str_opt = DecodeString(&cert_data.unencrypted_metadata_proto);
  if (!str_opt) return std::nullopt;
  nearby::sharing::proto::EncryptedMetadata unencrypted_metadata;
  if (!unencrypted_metadata.ParseFromString(*str_opt)) return std::nullopt;

  return NearbySharePrivateCertificate(
      static_cast<DeviceVisibility>(cert_data.visibility),
      absl::FromUnixNanos(cert_data.not_before),
      absl::FromUnixNanos(cert_data.not_after), std::move(key_pair),
      std::move(secret_key), std::move(metadata_encryption_key), std::move(id),
      std::move(unencrypted_metadata),
      StringToSalts(cert_data.consumed_salts));
}

std::optional<std::vector<uint8_t>>
NearbySharePrivateCertificate::GenerateUnusedSalt() {
  if (consumed_salts_.size() >= kNearbyShareMaxNumMetadataEncryptionKeySalts) {
    NL_LOG(ERROR) << "All salts exhausted for certificate.";
    return std::nullopt;
  }

  for (size_t attempt = 0;
       attempt < kNearbyShareMaxNumMetadataEncryptionKeySaltGenerationRetries;
       ++attempt) {
    std::vector<uint8_t> salt;
    if (next_salts_for_testing_.empty()) {
      salt = GenerateRandomBytes(2u);
    } else {
      salt = next_salts_for_testing_.front();
      next_salts_for_testing_.pop();
    }
    NL_DCHECK_EQ(2u, salt.size());

    if (consumed_salts_.find(salt) == consumed_salts_.end()) {
      consumed_salts_.insert(salt);
      return salt;
    }
  }

  NL_LOG(ERROR) << "Salt generation exceeded max number of retries. This is "
                   "highly improbable.";
  return std::nullopt;
}

std::optional<std::vector<uint8_t>>
NearbySharePrivateCertificate::EncryptMetadata() const {
  // Init() keeps a reference to the input key, so that reference must outlive
  // the lifetime of |aead|.
  std::vector<uint8_t> derived_key = DeriveNearbyShareKey(
      metadata_encryption_key_, kNearbyShareNumBytesAesGcmKey);

  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  aead.Init(derived_key);

  std::vector<uint8_t> metadata_array(unencrypted_metadata_.ByteSizeLong());
  unencrypted_metadata_.SerializeToArray(metadata_array.data(),
                                         metadata_array.size());

  return aead.Seal(
      metadata_array,
      /*nonce=*/
      DeriveNearbyShareKey(as_bytes(absl::MakeSpan(secret_key_->key())),
                           kNearbyShareNumBytesAesGcmIv),
      /*additional_data=*/absl::Span<const uint8_t>());
}

}  // namespace sharing
}  // namespace nearby
