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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_MANAGER_IMPL_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_MANAGER_IMPL_H_

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/die_if_null.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/credential_storage_impl.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/runnable.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/proto/credential.pb.h"
#include "presence/implementation/credential_manager.h"

namespace nearby {
namespace presence {

class CredentialManagerImpl : public CredentialManager {
 public:
  using IdentityType = ::nearby::internal::IdentityType;
  using Metadata = ::nearby::internal::Metadata;

  explicit CredentialManagerImpl(SingleThreadExecutor* executor)
      : executor_(ABSL_DIE_IF_NULL(executor)) {
    credential_storage_ptr_ = std::make_unique<nearby::CredentialStorageImpl>();
  }

  // Test purpose only.
  CredentialManagerImpl(
      SingleThreadExecutor* executor,
      std::unique_ptr<nearby::CredentialStorageImpl> credential_storage_ptr)
      : executor_(ABSL_DIE_IF_NULL(executor)),
        credential_storage_ptr_(std::move(credential_storage_ptr)) {}

  // AES only supports key sizes of 16, 24 or 32 bytes.
  static constexpr int kAuthenticityKeyByteSize = 32;

  // Length of key in bytes required by AES-GCM encryption.
  static constexpr size_t kNearbyPresenceNumBytesAesGcmKeySize = 32;

  // Modify this to 12 after use real AES.
  static constexpr int kAesGcmIVSize = 12;

  void GenerateCredentials(
      const Metadata& metadata, absl::string_view manager_app_id,
      const std::vector<nearby::internal::IdentityType>& identity_types,
      int credential_life_cycle_days, int contiguous_copy_of_credentials,
      GenerateCredentialsResultCallback credentials_generated_cb) override;

  void UpdateRemotePublicCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<nearby::internal::SharedCredential>&
          remote_public_creds,
      UpdateRemotePublicCredentialsCallback credentials_updated_cb) override;

  void UpdateLocalCredential(
      const CredentialSelector& credential_selector,
      nearby::internal::LocalCredential credential,
      SaveCredentialsResultCallback result_callback) override;

  void GetLocalCredentials(const CredentialSelector& credential_selector,
                           GetLocalCredentialsResultCallback callback) override;

  // Blocking version of `GetLocalCredentials`
  nearby::ExceptionOr<std::vector<nearby::internal::LocalCredential>>
  GetLocalCredentialsSync(const CredentialSelector& credential_selector,
                          absl::Duration timeout);

  // Used to fetch remote public creds when scanning.
  void GetPublicCredentials(
      const CredentialSelector& credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback) override;

  // Blocking version of `GetPublicCredentials`.
  ::nearby::ExceptionOr<std::vector<::nearby::internal::SharedCredential>>
  GetPublicCredentialsSync(const CredentialSelector& credential_selector,
                           PublicCredentialType public_credential_type,
                           absl::Duration timeout);

  SubscriberId SubscribeForPublicCredentials(
      const CredentialSelector& credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback) override;

  void UnsubscribeFromPublicCredentials(SubscriberId id) override;

  std::string DecryptMetadata(absl::string_view metadata_encryption_key,
                              absl::string_view key_seed,
                              absl::string_view metadata_string) override;

  std::pair<nearby::internal::LocalCredential,
            nearby::internal::SharedCredential>
  CreateLocalCredential(const Metadata& metadata, IdentityType identity_type,
                        absl::Time start_time, absl::Time end_time);

  nearby::internal::SharedCredential CreatePublicCredential(
      const nearby::internal::LocalCredential& private_credential,
      const Metadata& metadata, const std::vector<uint8_t>& public_key);

  virtual std::string EncryptMetadata(absl::string_view metadata_encryption_key,
                                      absl::string_view key_seed,
                                      absl::string_view metadata_string);

  // Extend the key from 16 bytes to 32 bytes.
  std::vector<uint8_t> ExtendMetadataEncryptionKey(
      absl::string_view metadata_encryption_key);

 private:
  struct SubscriberKey {
    CredentialSelector credential_selector;
    PublicCredentialType public_credential_type;
    template <typename H>
    friend H AbslHashValue(H h, const SubscriberKey& key) {
      return H::combine(std::move(h), key.credential_selector,
                        key.public_credential_type);
    }
    friend bool operator==(const SubscriberKey& a, const SubscriberKey& b) {
      return a.public_credential_type == b.public_credential_type &&
             a.credential_selector == b.credential_selector;
    }
  };
  class Subscriber {
   public:
    Subscriber(SubscriberId id, GetPublicCredentialsResultCallback callback)
        : callback_(std::move(callback)), id_(id) {}

    SubscriberId GetId() const { return id_; }

    // Notifies the subscriber about fetched credentials.
    void NotifyCredentialsFetched(
        std::vector<::nearby::internal::SharedCredential>& credentials);

   private:
    GetPublicCredentialsResultCallback callback_;
    SubscriberId id_;
  };

  void RunOnServiceControllerThread(absl::string_view name,
                                    Runnable&& runnable) {
    executor_->Execute(std::string(name), std::move(runnable));
  }
  void OnCredentialsChanged(absl::string_view manager_app_id,
                            absl::string_view account_name,
                            PublicCredentialType credential_type)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  void NotifySubscribers(
      const SubscriberKey& key,
      std::vector<::nearby::internal::SharedCredential> credentials)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  void AddSubscriber(SubscriberKey key, SubscriberId id,
                     GetPublicCredentialsResultCallback callback)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  void RemoveSubscriber(SubscriberId id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  absl::flat_hash_set<IdentityType> GetSubscribedIdentities(
      absl::string_view manager_app_id, absl::string_view account_name,
      PublicCredentialType credential_type) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  GetPublicCredentialsResultCallback CreateNotifySubscribersCallback(
      SubscriberKey key);

  absl::flat_hash_map<SubscriberKey, std::vector<Subscriber>> subscribers_
      ABSL_GUARDED_BY(*executor_);
  SingleThreadExecutor* executor_;
  std::unique_ptr<nearby::CredentialStorageImpl> credential_storage_ptr_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_MANAGER_IMPL_H_
