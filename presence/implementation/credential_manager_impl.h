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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/die_if_null.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/credential_storage_impl.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/runnable.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/metadata.pb.h"
#include "presence/implementation/credential_manager.h"

namespace nearby {
namespace presence {

class CredentialManagerImpl : public CredentialManager {
 public:
  using IdentityType = ::nearby::internal::IdentityType;
  using DeviceIdentityMetaData = ::nearby::internal::DeviceIdentityMetaData;

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
      const DeviceIdentityMetaData& device_identity_metadata,
      absl::string_view manager_app_id,
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

  // Used to fetch local/remote public creds based on the value of
  // public_credential_type.
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

  std::string DecryptDeviceIdentityMetaData(
      absl::string_view metadata_encryption_key, absl::string_view key_seed,
      absl::string_view metadata_string) override;

  std::pair<nearby::internal::LocalCredential,
            nearby::internal::SharedCredential>
  CreateLocalCredential(const DeviceIdentityMetaData& device_identity_metadata,
                        IdentityType identity_type, absl::Time start_time,
                        absl::Time end_time);

  nearby::internal::SharedCredential CreatePublicCredential(
      const nearby::internal::LocalCredential& private_credential,
      const DeviceIdentityMetaData& device_identity_metadata,
      const std::vector<uint8_t>& public_key);

  virtual std::string EncryptDeviceIdentityMetaData(
      absl::string_view metadata_encryption_key, absl::string_view key_seed,
      absl::string_view metadata_string);

  // Extend the key from 16 bytes to 32 bytes.
  std::vector<uint8_t> ExtendMetadataEncryptionKey(
      absl::string_view metadata_encryption_key);

  void SetDeviceIdentityMetaData(
      const DeviceIdentityMetaData& device_identity_metadata,
      bool regen_credentials, absl::string_view manager_app_id,
      const std::vector<nearby::internal::IdentityType>& identity_types,
      int credential_life_cycle_days, int contiguous_copy_of_credentials,
      GenerateCredentialsResultCallback credentials_generated_cb) override {
    device_identity_metadata_ = device_identity_metadata;
    if (regen_credentials) {
      GenerateCredentials(device_identity_metadata, manager_app_id,
                          identity_types, credential_life_cycle_days,
                          contiguous_copy_of_credentials,
                          std::move(credentials_generated_cb));
    }
  }

  ::nearby::internal::DeviceIdentityMetaData GetDeviceIdentityMetaData()
      override {
    return device_identity_metadata_;
  }

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

  bool WaitForLatch(absl::string_view method_name, CountDownLatch* latch);

  // The similar flow to check-expired-then-refill-if-needed is needed in both
  // GetLocalCredentials() and GetPublicCredentials(). The high level flow is:
  // check if there're expired creds from the result credentials list from
  // GetLocal/GetPublic, if some creds expired, prune the expired, merge with
  // newly generated ones. Then get the corresponding(local/shared) creds list
  // from the storage, also prune expired, merge with newly
  // generated. Then finally, save the newly merged two lists (local & shared)
  // to storage. For re-use purpose, this private function is made to be able
  // to take in different parameters from both GetLocalCredentials() and
  // GetPublicCredentials().
  void CheckCredentialsAndRefillIfNeeded(
      const CredentialSelector& credential_selector,
      absl::variant<std::vector<nearby::internal::LocalCredential>*,
                    std::vector<nearby::internal::SharedCredential>*>
          credential_list_variant,
      std::optional<GetLocalCredentialsResultCallback>
          callback_for_local_credentials,
      std::optional<GetPublicCredentialsResultCallback>
          callback_for_shared_credentials);
  void RefillRemainingValidCredentialsWithNewCredentials(
      const CredentialSelector& credential_selector,
      std::vector<nearby::internal::LocalCredential> valid_local_credentials,
      std::vector<nearby::internal::SharedCredential> valid_shared_credentials,
      int64_t start_time_to_generate_new_credentials_millis,
      std::optional<GetLocalCredentialsResultCallback>
          callback_for_local_credentials,
      std::optional<GetPublicCredentialsResultCallback>
          callback_for_shared_credentials);
  void OnCredentialRefillComplete(
      absl::Status save_credentials_status,
      std::vector<nearby::internal::LocalCredential> valid_local_credentials,
      std::vector<nearby::internal::SharedCredential> valid_shared_credentials,
      std::optional<GetLocalCredentialsResultCallback>
          callback_for_local_credentials,
      std::optional<GetPublicCredentialsResultCallback>
          callback_for_shared_credentials);

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
  DeviceIdentityMetaData device_identity_metadata_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_MANAGER_IMPL_H_
