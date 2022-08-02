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

#include "third_party/nearby/presence/implementation/credential_manager_impl.h"

#include <string>
#include <vector>

#include "internal/crypto/ec_private_key.h"
#include "internal/crypto/encryptor.h"
#include "internal/crypto/symmetric_key.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/platform/logging.h"
#include "third_party/nearby/presence/encryption.h"

namespace nearby {
namespace presence {
namespace {
using ::location::nearby::Base64Utils;
using ::location::nearby::Crypto;
using ::nearby::presence::proto::PrivateCredential;
using ::nearby::presence::proto::PublicCredential;

// Key to retrieve local device's Private/Public Key Credentials from key store.
constexpr char kPairedKeyAliasPrefix[] = "nearby_presence_paired_key_alias_";

}  // namespace

const char CredentialManagerImpl::kIV[] = "the iv: 16 bytes";

void CredentialManagerImpl::GenerateCredentials(
    proto::DeviceMetadata device_metadata,
    std::vector<proto::IdentityType> identity_types,
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
      if (public_private_credentials.first != nullptr) {
        private_credentials.push_back(*public_private_credentials.first);
        public_credentials.push_back(*public_private_credentials.second);
      }
      start_time_millis += gap_millis;
      end_time_millis += gap_millis;
    }
  }
  // TODO(b/241488275) Store all the public and private credentials and call the
  // callback to inform client.
  credentials_generated_cb.credentials_generated_cb(public_credentials);
}

std::pair<std::unique_ptr<PrivateCredential>, std::unique_ptr<PublicCredential>>
CredentialManagerImpl::CreatePrivateCredential(
    proto::DeviceMetadata device_metadata, proto::IdentityType identity_type,
    uint64_t start_time_ms, uint64_t end_time_ms) {
  auto private_credential_ptr = std::make_unique<PrivateCredential>();
  auto private_credential = private_credential_ptr.get();
  private_credential->set_start_time_millis(start_time_ms);
  private_credential->set_end_time_millis(end_time_ms);
  private_credential->set_identity_type(identity_type);

  // Creates an AES key to encrypt the whole broadcast.
  std::string secret_key =
      Encryption::GenerateRandomByteArray(kAuthenticityKeyByteSize);
  private_credential->set_authenticity_key(secret_key);

  // Uses SHA-256 algorithm to generate the credential ID from the authenticity
  // key
  auto secret_id = Crypto::Sha256(secret_key);
  if (secret_id.Empty()) {
    NEARBY_LOG(ERROR,
               "Failed to create private credential because it failed to "
               "create a secret id.");
    return std::pair<std::unique_ptr<PrivateCredential>,
                     std::unique_ptr<PublicCredential>>(
        std::unique_ptr<PrivateCredential>(nullptr),
        std::unique_ptr<PublicCredential>(nullptr));
  }
  private_credential->set_secret_id(secret_id.AsStringView());

  std::string alias = Base64Utils::Encode(secret_id);
  auto prefixedAlias = kPairedKeyAliasPrefix + alias;

  // Generate key pair. Store the private key in private credential.
  auto key_pair = crypto::ECPrivateKey::Create();
  std::vector<uint8_t> private_key;
  key_pair->ExportPrivateKey(&private_key);
  private_credential->set_verification_key(
      std::string(private_key.begin(), private_key.end()));

  // Create an AES key to encrypt the device metadata.
  auto metadata_key =
      Encryption::GenerateRandomByteArray(kAuthenticityKeyByteSize);
  private_credential->set_metadata_encryption_key(metadata_key);

  // set device meta data
  *(private_credential->mutable_device_metadata()) = device_metadata;

  // Generate the public credential
  std::vector<uint8_t> public_key;
  key_pair->ExportPublicKey(&public_key);
  auto public_credential_ptr =
      CreatePublicCredential(private_credential_ptr.get(), &public_key);

  return std::pair<std::unique_ptr<PrivateCredential>,
                   std::unique_ptr<PublicCredential>>(
      std::move(private_credential_ptr), std::move(public_credential_ptr));
}

std::unique_ptr<proto::PublicCredential>
CredentialManagerImpl::CreatePublicCredential(
    proto::PrivateCredential* private_credential,
    std::vector<uint8_t>* public_key) {
  auto public_credential_ptr = std::make_unique<PublicCredential>();
  auto public_credential = public_credential_ptr.get();

  public_credential->set_identity_type(private_credential->identity_type());
  public_credential->set_secret_id(private_credential->secret_id());
  public_credential->set_authenticity_key(
      private_credential->authenticity_key());
  public_credential->set_start_time_millis(
      private_credential->start_time_millis());
  public_credential->set_end_time_millis(private_credential->end_time_millis());
  // set up the public key
  public_credential->set_verification_key(
      std::string(public_key->begin(), public_key->end()));

  auto metadata_encryption_key_tag =
      Crypto::Sha256(private_credential->metadata_encryption_key());
  public_credential->set_metadata_encryption_key_tag(
      metadata_encryption_key_tag.AsStringView());

  // Encrypt the device metadata
  crypto::Encryptor encryptor;
  auto sym_key = crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, private_credential->metadata_encryption_key());

  // It is GCM in the spec. Here we use CBC instead since GCM is not supported
  // now.
  if (!encryptor.Init(sym_key.get(), crypto::Encryptor::CBC, kIV)) {
    NEARBY_LOG(ERROR, "Fails to initialize the encryptor");
    return std::unique_ptr<PublicCredential>(nullptr);
  }

  encryptor.Encrypt(private_credential->device_metadata().SerializeAsString(),
                    public_credential->mutable_encrypted_metadata_bytes());
  return public_credential_ptr;
}

}  // namespace presence
}  // namespace nearby
