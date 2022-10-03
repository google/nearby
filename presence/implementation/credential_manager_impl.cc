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

#include "presence/implementation/credential_manager_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/crypto/aead.h"
#include "internal/crypto/ec_private_key.h"
#include "internal/crypto/hkdf.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/platform/logging.h"
#include "internal/proto/credential.proto.h"
#include "presence/implementation/encryption.h"

namespace nearby {
namespace presence {
namespace {
using ::location::nearby::Base64Utils;
using ::location::nearby::Crypto;
using ::nearby::internal::DeviceMetadata;
using ::nearby::internal::IdentityType;
using ::nearby::internal::PrivateCredential;
using ::nearby::internal::PublicCredential;

// Key to retrieve local device's Private/Public Key Credentials from key store.
constexpr char kPairedKeyAliasPrefix[] = "nearby_presence_paired_key_alias_";

}  // namespace

void CredentialManagerImpl::GenerateCredentials(
    const DeviceMetadata& device_metadata, absl::string_view manager_app_id,
    const std::vector<IdentityType>& identity_types,
    int credential_life_cycle_days, int contiguous_copy_of_credentials,
    GenerateCredentialsCallback credentials_generated_cb) {
  std::vector<PublicCredential> public_credentials;
  std::vector<PrivateCredential> private_credentials;

  for (auto identity_type : identity_types) {
    // TODO(b/241587906): Get linux time from the platform (like Android)
    uint64_t start_time_millis = 0;
    const uint64_t gap_millis = credential_life_cycle_days * 24 * 3600 * 1000;
    uint64_t end_time_millis = start_time_millis + gap_millis;

    for (int index = 0; index < contiguous_copy_of_credentials; index++) {
      auto public_private_credentials = CreatePrivateCredential(
          device_metadata, identity_type, start_time_millis, end_time_millis);
      if (public_private_credentials.second.identity_type() !=
          IdentityType::IDENTITY_TYPE_UNSPECIFIED) {
        private_credentials.push_back(public_private_credentials.first);
        public_credentials.push_back(public_private_credentials.second);
      }
      start_time_millis += gap_millis;
      end_time_millis += gap_millis;
    }
  }

  // Create credential_storage object and invoke SaveCredentials.
  credential_storage_ptr_->SaveCredentials(
      manager_app_id, device_metadata.account_name(), private_credentials,
      public_credentials, PublicCredentialType::kLocalPublicCredential,
      std::move(credentials_generated_cb));
}

std::pair<PrivateCredential, PublicCredential>
CredentialManagerImpl::CreatePrivateCredential(
    const DeviceMetadata& device_metadata, IdentityType identity_type,
    uint64_t start_time_ms, uint64_t end_time_ms) {
  PrivateCredential private_credential;
  private_credential.set_start_time_millis(start_time_ms);
  private_credential.set_end_time_millis(end_time_ms);
  private_credential.set_identity_type(identity_type);

  // Creates an AES key to encrypt the whole broadcast.
  std::string secret_key =
      Encryption::GenerateRandomByteArray(kAuthenticityKeyByteSize);
  private_credential.set_authenticity_key(secret_key);

  // Uses SHA-256 algorithm to generate the credential ID from the authenticity
  // key
  auto secret_id = Crypto::Sha256(secret_key);
  // Does not expect to fail here since Crypto::Sha256 should not return empty
  // ByteArray.
  CHECK(!secret_id.Empty()) << "Crypto::Sha256 failed!";

  private_credential.set_secret_id(secret_id.AsStringView());

  std::string alias = Base64Utils::Encode(secret_id);
  auto prefixedAlias = kPairedKeyAliasPrefix + alias;

  // Generate key pair. Store the private key in private credential.
  auto key_pair = crypto::ECPrivateKey::Create();
  std::vector<uint8_t> private_key;
  key_pair->ExportPrivateKey(&private_key);
  private_credential.set_verification_key(
      std::string(private_key.begin(), private_key.end()));

  // Create an AES key to encrypt the device metadata.
  auto metadata_key =
      Encryption::GenerateRandomByteArray(kAuthenticityKeyByteSize);
  private_credential.set_metadata_encryption_key(metadata_key);

  // set device meta data
  *(private_credential.mutable_device_metadata()) = device_metadata;

  // Generate the public credential
  std::vector<uint8_t> public_key;
  key_pair->ExportPublicKey(&public_key);

  return std::pair<PrivateCredential, PublicCredential>(
      private_credential,
      CreatePublicCredential(private_credential, public_key));
}

PublicCredential CredentialManagerImpl::CreatePublicCredential(
    const PrivateCredential& private_credential,
    const std::vector<uint8_t>& public_key) {
  PublicCredential public_credential;
  public_credential.set_identity_type(private_credential.identity_type());
  public_credential.set_secret_id(private_credential.secret_id());
  public_credential.set_authenticity_key(private_credential.authenticity_key());
  public_credential.set_start_time_millis(
      private_credential.start_time_millis());
  public_credential.set_end_time_millis(private_credential.end_time_millis());
  // set up the public key
  public_credential.set_verification_key(
      std::string(public_key.begin(), public_key.end()));

  auto metadata_encryption_key_tag =
      Crypto::Sha256(private_credential.metadata_encryption_key());
  public_credential.set_metadata_encryption_key_tag(
      metadata_encryption_key_tag.AsStringView());

  // Encrypt the device metadata
  auto encrypted_meta_data = EncryptDeviceMetadata(
      private_credential.metadata_encryption_key(),
      private_credential.authenticity_key(),
      private_credential.device_metadata().SerializeAsString());

  if (encrypted_meta_data.empty()) {
    NEARBY_LOGS(ERROR) << "Fails to encrypt the device metadata.";
    public_credential.set_identity_type(
        IdentityType::IDENTITY_TYPE_UNSPECIFIED);
    return public_credential;
  }

  public_credential.set_encrypted_metadata_bytes(encrypted_meta_data);
  return public_credential;
}

std::string CredentialManagerImpl::DecryptDeviceMetadata(
    absl::string_view device_metadata_encryption_key,
    absl::string_view authenticity_key,
    absl::string_view device_metadata_string) {
  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);

  std::vector<uint8_t> derived_key =
      ExtendMetadataEncryptionKey(device_metadata_encryption_key);
  aead.Init(derived_key);

  auto iv = Encryption::CustomizeBytesSize(
      authenticity_key, CredentialManagerImpl::kAesGcmIVSize);
  std::vector<uint8_t> iv_bytes(iv.begin(), iv.end());
  std::vector<uint8_t> encrypted_device_metadata_bytes(
      device_metadata_string.begin(), device_metadata_string.end());

  auto result = aead.Open(encrypted_device_metadata_bytes,
                          /*nonce=*/
                          iv_bytes,
                          /*additional_data=*/absl::Span<uint8_t>());

  return std::string(result.value().begin(), result.value().end());
}

std::string CredentialManagerImpl::EncryptDeviceMetadata(
    absl::string_view device_metadata_encryption_key,
    absl::string_view authenticity_key,
    absl::string_view device_metadata_string) {
  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);

  std::vector<uint8_t> derived_key =
      ExtendMetadataEncryptionKey(device_metadata_encryption_key);

  aead.Init(derived_key);

  auto iv = Encryption::CustomizeBytesSize(authenticity_key, kAesGcmIVSize);
  std::vector<uint8_t> iv_bytes(iv.begin(), iv.end());

  std::vector<uint8_t> device_metadata_bytes(device_metadata_string.begin(),
                                             device_metadata_string.end());
  device_metadata_bytes.resize(device_metadata_string.size());

  auto encrypted = aead.Seal(device_metadata_bytes,
                             /*nonce=*/
                             iv_bytes,
                             /*additional_data=*/absl::Span<uint8_t>());

  return std::string(encrypted.begin(), encrypted.end());
}

std::vector<uint8_t> CredentialManagerImpl::ExtendMetadataEncryptionKey(
    absl::string_view device_metadata_encryption_key) {
  return crypto::HkdfSha256(
      std::vector<uint8_t>(device_metadata_encryption_key.begin(),
                           device_metadata_encryption_key.end()),
      /*salt=*/absl::Span<uint8_t>(),
      /*info=*/absl::Span<uint8_t>(), kNearbyPresenceNumBytesAesGcmKeySize);
}

void CredentialManagerImpl::GetPrivateCredentials(
    const CredentialSelector& credential_selector,
    GetPrivateCredentialsResultCallback callback) {
  credential_storage_ptr_->GetPrivateCredentials(credential_selector,
                                                 std::move(callback));
}

void CredentialManagerImpl::GetPublicCredentials(
    const CredentialSelector& credential_selector,
    PublicCredentialType public_credential_type,
    GetPublicCredentialsResultCallback callback) {
  credential_storage_ptr_->GetPublicCredentials(
      credential_selector, public_credential_type, std::move(callback));
}

}  // namespace presence
}  // namespace nearby
