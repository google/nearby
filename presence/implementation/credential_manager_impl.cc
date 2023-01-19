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
#include "internal/proto/credential.proto.h"
#include "presence/implementation/encryption.h"
#include "presence/implementation/ldt.h"

namespace nearby {
namespace presence {
namespace {
using ::nearby::Base64Utils;
using ::nearby::Crypto;
using ::nearby::Exception;
using ::nearby::ExceptionOr;
using ::nearby::Future;
using ::nearby::internal::DeviceMetadata;
using ::nearby::internal::IdentityType;
using ::nearby::internal::LocalCredential;
using ::nearby::internal::SharedCredential;

// Key to retrieve local device's Private/Public Key Credentials from key store.
constexpr char kPairedKeyAliasPrefix[] = "nearby_presence_paired_key_alias_";

constexpr absl::Duration kTimeout = absl::Seconds(3);

}  // namespace

void CredentialManagerImpl::GenerateCredentials(
    const DeviceMetadata& device_metadata, absl::string_view manager_app_id,
    const std::vector<IdentityType>& identity_types,
    int credential_life_cycle_days, int contiguous_copy_of_credentials,
    GenerateCredentialsResultCallback credentials_generated_cb) {
  std::vector<SharedCredential> public_credentials;
  std::vector<LocalCredential> private_credentials;

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
      SaveCredentialsResultCallback{
          .credentials_saved_cb =
              [this, manager_app_id = std::string(manager_app_id),
               account_name = device_metadata.account_name(),
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
    const std::vector<nearby::internal::SharedCredential>& remote_public_creds,
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
CredentialManagerImpl::CreatePrivateCredential(
    const DeviceMetadata& device_metadata, IdentityType identity_type,
    uint64_t start_time_ms, uint64_t end_time_ms) {
  LocalCredential private_credential;
  private_credential.set_start_time_millis(start_time_ms);
  private_credential.set_end_time_millis(end_time_ms);
  private_credential.set_identity_type(identity_type);

  // Creates an AES key to encrypt the whole broadcast.
  std::string secret_key =
      Encryption::GenerateRandomByteArray(kAuthenticityKeyByteSize);
  private_credential.set_authenticity_key(secret_key);

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

  return std::pair<LocalCredential, SharedCredential>(
      private_credential,
      CreatePublicCredential(private_credential, public_key));
}

SharedCredential CredentialManagerImpl::CreatePublicCredential(
    const LocalCredential& private_credential,
    const std::vector<uint8_t>& public_key) {
  SharedCredential public_credential;
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
      std::string(metadata_encryption_key_tag.AsStringView()));

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

ExceptionOr<std::vector<LocalCredential>>
CredentialManagerImpl::GetPrivateCredentialsSync(
    const CredentialSelector& credential_selector, absl::Duration timeout) {
  Future<std::vector<LocalCredential>> result;
  GetPrivateCredentials(
      credential_selector,
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
          [this, key](
              absl::StatusOr<std::vector<::nearby::internal::SharedCredential>>
                  credentials) {
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
    const SubscriberKey& key,
    std::vector<::nearby::internal::SharedCredential> credentials) {
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
    std::vector<::nearby::internal::SharedCredential>& credentials) {
  callback_.credentials_fetched_cb(credentials);
}

}  // namespace presence
}  // namespace nearby
