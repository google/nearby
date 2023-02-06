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

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/crypto/aead.h"
#include "internal/crypto/ec_private_key.h"
#include "internal/crypto/hkdf.h"
#include "internal/crypto/random.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/future.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/platform/logging.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/local_credential.pb.h"
#include "presence/implementation/base_broadcast_request.h"
#include "presence/implementation/ldt.h"

namespace nearby {
namespace presence {
namespace {
using ::nearby::Base64Utils;
using ::nearby::Crypto;
using ::nearby::Exception;
using ::nearby::ExceptionOr;
using ::nearby::Future;
using ::nearby::internal::IdentityType;
using ::nearby::internal::LocalCredential;
using ::nearby::internal::SharedCredential;

// Key to retrieve local device's Private/Public Key Credentials from key store.
constexpr char kPairedKeyAliasPrefix[] = "nearby_presence_paired_key_alias_";

// Returns a random duration in [0, max_duration] range.
absl::Duration RandomDuration(absl::Duration max_duration) {
  uint32_t random = ::crypto::RandData<uint32_t>();
  return max_duration * random / std::numeric_limits<uint32_t>::max();
}

std::string CustomizeBytesSize(absl::string_view bytes, size_t len) {
  return ::crypto::HkdfSha256(
      /*ikm=*/bytes,
      /*salt=*/std::string(CredentialManagerImpl::kAuthenticityKeyByteSize, 0),
      /*info=*/"", /*derived_key_size=*/len);
}

}  // namespace

void CredentialManagerImpl::GenerateCredentials(
    const Metadata& metadata, absl::string_view manager_app_id,
    const std::vector<IdentityType>& identity_types,
    int credential_life_cycle_days, int contiguous_copy_of_credentials,
    GenerateCredentialsResultCallback credentials_generated_cb) {
  std::vector<SharedCredential> public_credentials;
  std::vector<LocalCredential> private_credentials;

  for (auto identity_type : identity_types) {
    absl::Time start_time = SystemClock::ElapsedRealtime();
    absl::Duration gap = credential_life_cycle_days * absl::Hours(24);
    for (int index = 0; index < contiguous_copy_of_credentials; index++) {
      auto public_private_credentials = CreateLocalCredential(
          metadata, identity_type, start_time, start_time + gap);
      if (public_private_credentials.second.identity_type() !=
          IdentityType::IDENTITY_TYPE_UNSPECIFIED) {
        private_credentials.push_back(public_private_credentials.first);
        public_credentials.push_back(public_private_credentials.second);
      }
      start_time += gap;
    }
  }

  // Create credential_storage object and invoke SaveCredentials.
  credential_storage_ptr_->SaveCredentials(
      manager_app_id, metadata.account_name(), private_credentials,
      public_credentials, PublicCredentialType::kLocalPublicCredential,
      SaveCredentialsResultCallback{
          .credentials_saved_cb =
              [this, manager_app_id = std::string(manager_app_id),
               account_name = metadata.account_name(),
               callback = std::move(credentials_generated_cb),
               public_credentials](absl::Status status) mutable {
                if (!status.ok()) {
                  NEARBY_LOGS(WARNING)
                      << "Save credentials failed with: " << status;
                  std::move(callback.credentials_generated_cb)(status);
                  return;
                }
                std::move(callback.credentials_generated_cb)(
                    std::move(public_credentials));
                RunOnServiceControllerThread(
                    "local-creds-changed",
                    [this, manager_app_id = std::string(manager_app_id),
                     account_name = std::string(account_name)]()
                        ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                          OnCredentialsChanged(
                              manager_app_id, account_name,
                              PublicCredentialType::kLocalPublicCredential);
                        });
              }});
}

void CredentialManagerImpl::UpdateRemotePublicCredentials(
    absl::string_view manager_app_id, absl::string_view account_name,
    const std::vector<SharedCredential>& remote_public_creds,
    UpdateRemotePublicCredentialsCallback credentials_updated_cb) {
  credential_storage_ptr_->SaveCredentials(
      manager_app_id, account_name, /* private_credentials */ {},
      remote_public_creds, PublicCredentialType::kRemotePublicCredential,
      SaveCredentialsResultCallback{
          .credentials_saved_cb =
              [this, manager_app_id = std::string(manager_app_id),
               account_name = std::string(account_name),
               callback = std::move(credentials_updated_cb)](
                  absl::Status status) mutable {
                if (!status.ok()) {
                  NEARBY_LOGS(WARNING)
                      << "Update remote credentials failed with: " << status;
                } else {
                  RunOnServiceControllerThread(
                      "remote-creds-changed",
                      [this, manager_app_id = std::string(manager_app_id),
                       account_name = std::string(account_name)]()
                          ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                            OnCredentialsChanged(
                                manager_app_id, account_name,
                                PublicCredentialType::kRemotePublicCredential);
                          });
                }
                std::move(callback.credentials_updated_cb)(status);
              }});
}

std::pair<LocalCredential, SharedCredential>
CredentialManagerImpl::CreateLocalCredential(const Metadata& metadata,
                                             IdentityType identity_type,
                                             absl::Time start_time,
                                             absl::Time end_time) {
  LocalCredential private_credential;
  private_credential.set_start_time_millis(absl::ToUnixMillis(start_time));
  private_credential.set_end_time_millis(absl::ToUnixMillis(end_time));
  private_credential.set_identity_type(identity_type);

  // Creates an AES key to encrypt the whole broadcast.
  std::string secret_key = crypto::RandBytes(kAuthenticityKeyByteSize);
  private_credential.set_key_seed(secret_key);

  // Uses SHA-256 algorithm to generate the credential ID from the
  // authenticity key
  auto secret_id = Crypto::Sha256(secret_key);
  // Does not expect to fail here since Crypto::Sha256 should not return
  // empty ByteArray.
  CHECK(!secret_id.Empty()) << "Crypto::Sha256 failed!";

  private_credential.set_secret_id(std::string(secret_id.AsStringView()));

  std::string alias = Base64Utils::Encode(secret_id);
  auto prefixedAlias = kPairedKeyAliasPrefix + alias;

  // Generate key pair. Store the private key in private credential.
  auto key_pair = crypto::ECPrivateKey::Create();
  std::vector<uint8_t> private_key;
  key_pair->ExportPrivateKey(&private_key);
  private_credential.mutable_connection_signing_key()->set_key(
      std::string(private_key.begin(), private_key.end()));
  // Create an AES key to encrypt the device metadata.
  auto metadata_key = crypto::RandBytes(kBaseMetadataSize);
  private_credential.set_metadata_encryption_key(metadata_key);

  // Generate the public credential
  std::vector<uint8_t> public_key;
  key_pair->ExportPublicKey(&public_key);

  return std::pair<LocalCredential, SharedCredential>(
      private_credential,
      CreatePublicCredential(private_credential, metadata, public_key));
}

SharedCredential CredentialManagerImpl::CreatePublicCredential(
    const LocalCredential& private_credential, const Metadata& metadata,
    const std::vector<uint8_t>& public_key) {
  // The start time in the public credential should be decreased by a random
  // value in 0 - 3 hours range.
  // The end time should be increased by a random value in 0 - 3 hours range.
  // This improves privacy by making it harder to correlate certificates.
  absl::Time start_time =
      absl::FromUnixMillis(private_credential.start_time_millis()) -
      RandomDuration(absl::Hours(3));
  absl::Time end_time =
      absl::FromUnixMillis(private_credential.end_time_millis()) +
      RandomDuration(absl::Hours(3));
  SharedCredential public_credential;
  public_credential.set_identity_type(private_credential.identity_type());
  public_credential.set_secret_id(private_credential.secret_id());
  public_credential.set_key_seed(private_credential.key_seed());
  public_credential.set_start_time_millis(absl::ToUnixMillis(start_time));
  public_credential.set_end_time_millis(absl::ToUnixMillis(end_time));
  // Set up the public key. Note, we are setting the "connection" key but we are
  // not setting the "advertisement" key because the latter is not used yet.
  public_credential.set_connection_signature_verification_key(
      std::string(public_key.begin(), public_key.end()));

  auto metadata_encryption_key_tag =
      Crypto::Sha256(private_credential.metadata_encryption_key());
  public_credential.set_metadata_encryption_key_tag(
      std::string(metadata_encryption_key_tag.AsStringView()));

  // Encrypt the device metadata
  auto encrypted_meta_data = EncryptMetadata(
      private_credential.metadata_encryption_key(),
      private_credential.key_seed(), metadata.SerializeAsString());

  if (encrypted_meta_data.empty()) {
    NEARBY_LOGS(ERROR) << "Fails to encrypt the device metadata.";
    public_credential.set_identity_type(
        IdentityType::IDENTITY_TYPE_UNSPECIFIED);
    return public_credential;
  }

  public_credential.set_encrypted_metadata_bytes(encrypted_meta_data);
  return public_credential;
}

std::string CredentialManagerImpl::DecryptMetadata(
    absl::string_view metadata_encryption_key, absl::string_view key_seed,
    absl::string_view metadata_string) {
  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);

  std::vector<uint8_t> derived_key =
      ExtendMetadataEncryptionKey(metadata_encryption_key);
  aead.Init(derived_key);

  auto iv = CustomizeBytesSize(key_seed, CredentialManagerImpl::kAesGcmIVSize);
  std::vector<uint8_t> iv_bytes(iv.begin(), iv.end());
  std::vector<uint8_t> encrypted_metadata_bytes(metadata_string.begin(),
                                                metadata_string.end());

  auto result = aead.Open(encrypted_metadata_bytes,
                          /*nonce=*/
                          iv_bytes,
                          /*additional_data=*/absl::Span<uint8_t>());

  return std::string(result.value().begin(), result.value().end());
}

std::string CredentialManagerImpl::EncryptMetadata(
    absl::string_view metadata_encryption_key, absl::string_view key_seed,
    absl::string_view metadata_string) {
  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);

  std::vector<uint8_t> derived_key =
      ExtendMetadataEncryptionKey(metadata_encryption_key);

  aead.Init(derived_key);

  auto iv = CustomizeBytesSize(key_seed, kAesGcmIVSize);
  std::vector<uint8_t> iv_bytes(iv.begin(), iv.end());

  std::vector<uint8_t> metadata_bytes(metadata_string.begin(),
                                      metadata_string.end());
  metadata_bytes.resize(metadata_string.size());

  auto encrypted = aead.Seal(metadata_bytes,
                             /*nonce=*/
                             iv_bytes,
                             /*additional_data=*/absl::Span<uint8_t>());

  return std::string(encrypted.begin(), encrypted.end());
}

std::vector<uint8_t> CredentialManagerImpl::ExtendMetadataEncryptionKey(
    absl::string_view metadata_encryption_key) {
  return crypto::HkdfSha256(
      std::vector<uint8_t>(metadata_encryption_key.begin(),
                           metadata_encryption_key.end()),
      /*salt=*/absl::Span<uint8_t>(),
      /*info=*/absl::Span<uint8_t>(), kNearbyPresenceNumBytesAesGcmKeySize);
}

void CredentialManagerImpl::GetLocalCredentials(
    const CredentialSelector& credential_selector,
    GetLocalCredentialsResultCallback callback) {
  credential_storage_ptr_->GetLocalCredentials(credential_selector,
                                               std::move(callback));
}

void CredentialManagerImpl::GetPublicCredentials(
    const CredentialSelector& credential_selector,
    PublicCredentialType public_credential_type,
    GetPublicCredentialsResultCallback callback) {
  credential_storage_ptr_->GetPublicCredentials(
      credential_selector, public_credential_type, std::move(callback));
}

ExceptionOr<std::vector<LocalCredential>>
CredentialManagerImpl::GetLocalCredentialsSync(
    const CredentialSelector& credential_selector, absl::Duration timeout) {
  Future<std::vector<LocalCredential>> result;
  GetLocalCredentials(credential_selector,
                      {.credentials_fetched_cb =
                           [result](absl::StatusOr<std::vector<LocalCredential>>
                                        credentials) mutable {
                             if (!credentials.ok()) {
                               result.SetException({Exception::kFailed});
                             } else {
                               result.Set(std::move(*credentials));
                             }
                           }});
  return result.Get(timeout);
}

ExceptionOr<std::vector<SharedCredential>>
CredentialManagerImpl::GetPublicCredentialsSync(
    const CredentialSelector& credential_selector,
    PublicCredentialType public_credential_type, absl::Duration timeout) {
  Future<std::vector<SharedCredential>> result;
  GetPublicCredentials(
      credential_selector, public_credential_type,
      {.credentials_fetched_cb =
           [result](absl::StatusOr<std::vector<SharedCredential>>
                        credentials) mutable {
             if (!credentials.ok()) {
               result.SetException({Exception::kFailed});
             } else {
               result.Set(std::move(*credentials));
             }
           }});
  return result.Get(timeout);
}

SubscriberId CredentialManagerImpl::SubscribeForPublicCredentials(
    const CredentialSelector& credential_selector,
    PublicCredentialType public_credential_type,
    GetPublicCredentialsResultCallback callback) {
  SubscriberId id = ::crypto::RandData<SubscriberId>();
  RunOnServiceControllerThread(
      "add-subscriber",
      [this, key = SubscriberKey{credential_selector, public_credential_type},
       id, callback = std::move(callback)]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) mutable {
            AddSubscriber(key, id, std::move(callback));
          });
  GetPublicCredentials(credential_selector, public_credential_type,
                       CreateNotifySubscribersCallback(
                           {credential_selector, public_credential_type}));
  return id;
}

void CredentialManagerImpl::UnsubscribeFromPublicCredentials(SubscriberId id) {
  RunOnServiceControllerThread("remove-subscriber",
                               [this, id]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(
                                   *executor_) { RemoveSubscriber(id); });
}

void CredentialManagerImpl::AddSubscriber(
    SubscriberKey key, SubscriberId id,
    GetPublicCredentialsResultCallback callback) {
  subscribers_[key].push_back(Subscriber(id, std::move(callback)));
}

void CredentialManagerImpl::RemoveSubscriber(SubscriberId id) {
  for (auto& entry : subscribers_) {
    auto it = std::find_if(
        entry.second.begin(), entry.second.end(),
        [&](Subscriber& subscriber) { return subscriber.GetId() == id; });
    if (it != entry.second.end()) {
      entry.second.erase(it);
      if (subscribers_[entry.first].empty()) {
        subscribers_.erase(entry.first);
      }
      return;
    }
  }
}

absl::flat_hash_set<IdentityType>
CredentialManagerImpl::GetSubscribedIdentities(
    absl::string_view manager_app_id, absl::string_view account_name,
    PublicCredentialType credential_type) const {
  absl::flat_hash_set<IdentityType> identities;
  for (auto& entry : subscribers_) {
    const SubscriberKey& key = entry.first;
    if (key.public_credential_type == credential_type &&
        key.credential_selector.manager_app_id == manager_app_id &&
        key.credential_selector.account_name == account_name) {
      identities.insert(key.credential_selector.identity_type);
    }
  }
  return identities;
}

void CredentialManagerImpl::OnCredentialsChanged(
    absl::string_view manager_app_id, absl::string_view account_name,
    PublicCredentialType credential_type) {
  NEARBY_LOGS(INFO) << "OnCredentialsChanged for app " << manager_app_id
                    << ", account " << account_name;
  for (IdentityType identity_type :
       GetSubscribedIdentities(manager_app_id, account_name, credential_type)) {
    CredentialSelector credential_selector = {
        .manager_app_id = std::string(manager_app_id),
        .account_name = std::string(account_name),
        .identity_type = identity_type};
    GetPublicCredentials(credential_selector, credential_type,
                         CreateNotifySubscribersCallback(
                             {credential_selector, credential_type}));
  }
}

GetPublicCredentialsResultCallback
CredentialManagerImpl::CreateNotifySubscribersCallback(SubscriberKey key) {
  return GetPublicCredentialsResultCallback{
      .credentials_fetched_cb =
          [this,
           key](absl::StatusOr<std::vector<SharedCredential>> credentials) {
            if (!credentials.ok()) {
              NEARBY_LOGS(WARNING)
                  << "Failed to get public credentials: error code: "
                  << credentials.status();
              return;
            }
            RunOnServiceControllerThread(
                "notify-subscribers",
                [this, key, credentials = std::move(*credentials)]()
                    ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                      NotifySubscribers(key, credentials);
                    });
          }};
}

void CredentialManagerImpl::NotifySubscribers(
    const SubscriberKey& key, std::vector<SharedCredential> credentials) {
  // We are on `executor_` thread, so we can iterate over `subscribers_`
  // without locking.
  auto it = subscribers_.find(key);
  if (it == subscribers_.end()) {
    NEARBY_LOGS(WARNING)
        << "No subscribers for (app: " << key.credential_selector.manager_app_id
        << ", account: " << key.credential_selector.account_name
        << ", identity type: "
        << static_cast<int>(key.credential_selector.identity_type)
        << ", credential type: " << static_cast<int>(key.public_credential_type)
        << ")";
    return;
  }
  for (auto& subscriber : it->second) {
    subscriber.NotifyCredentialsFetched(credentials);
  }
}

void CredentialManagerImpl::Subscriber::NotifyCredentialsFetched(
    std::vector<SharedCredential>& credentials) {
  callback_.credentials_fetched_cb(credentials);
}

void CredentialManagerImpl::UpdateLocalCredential(
    const CredentialSelector& credential_selector,
    nearby::internal::LocalCredential credential,
    SaveCredentialsResultCallback result_callback) {
  credential_storage_ptr_->UpdateLocalCredential(
      credential_selector.manager_app_id, credential_selector.account_name,
      std::move(credential), std::move(result_callback));
}
}  // namespace presence
}  // namespace nearby
