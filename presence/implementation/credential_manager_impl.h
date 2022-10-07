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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/credential_storage_impl.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/proto/credential.pb.h"
#include "presence/implementation/credential_manager.h"

namespace nearby {
namespace presence {

class CredentialManagerImpl : public CredentialManager {
 public:
  CredentialManagerImpl() {
    credential_storage_ptr_ =
        std::make_unique<location::nearby::CredentialStorageImpl>();
  }

  // Test purpose only.
  explicit CredentialManagerImpl(
      std::unique_ptr<location::nearby::CredentialStorageImpl>
          credential_storage_ptr)
      : credential_storage_ptr_(std::move(credential_storage_ptr)) {}

  // AES only supports key sizes of 16, 24 or 32 bytes.
  static constexpr int kAuthenticityKeyByteSize = 16;

  // Length of key in bytes required by AES-GCM encryption.
  static constexpr size_t kNearbyPresenceNumBytesAesGcmKeySize = 32;

  // Modify this to 12 after use real AES.
  static constexpr int kAesGcmIVSize = 12;

  void GenerateCredentials(
      const nearby::internal::DeviceMetadata& device_metadata,
      absl::string_view manager_app_id,
      const std::vector<nearby::internal::IdentityType>& identity_types,
      int credential_life_cycle_days, int contiguous_copy_of_credentials,
      GenerateCredentialsCallback credentials_generated_cb) override;

  void UpdateRemotePublicCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<nearby::internal::PublicCredential>&
          remote_public_creds,
      UpdateRemotePublicCredentialsCallback credentials_updated_cb) override{};

  void GetPrivateCredentials(
      const CredentialSelector& credential_selector,
      GetPrivateCredentialsResultCallback callback) override;

  // Used to fetch remote public creds when scanning.
  void GetPublicCredentials(
      const CredentialSelector& credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback) override;

  std::string DecryptDeviceMetadata(
      absl::string_view device_metadata_encryption_key,
      absl::string_view authenticity_key,
      absl::string_view device_metadata_string) override;

  absl::StatusOr<std::string> DecryptDataElements(
      absl::string_view salt, absl::string_view data_elements) override {
    return absl::UnimplementedError("DecryptDataElements unimplemented");
  }

  absl::StatusOr<std::string> EncryptDataElements(
      nearby::internal::IdentityType identity, absl::string_view salt,
      absl::string_view data_elements) override {
    return absl::UnimplementedError("EncryptDataElements unimplemented");
  }

  std::pair<nearby::internal::PrivateCredential,
            nearby::internal::PublicCredential>
  CreatePrivateCredential(
      const nearby::internal::DeviceMetadata& device_metadata,
      nearby::internal::IdentityType identity_type, uint64_t start_time_ms,
      uint64_t end_time_ms);

  nearby::internal::PublicCredential CreatePublicCredential(
      const nearby::internal::PrivateCredential& private_credential,
      const std::vector<uint8_t>& public_key);

  virtual std::string EncryptDeviceMetadata(
      absl::string_view device_metadata_encryption_key,
      absl::string_view authenticity_key,
      absl::string_view device_metadata_string);

  // Extend the key from 16 bytes to 32 bytes.
  std::vector<uint8_t> ExtendMetadataEncryptionKey(
      absl::string_view device_metadata_encryption_key);

 private:
  std::unique_ptr<location::nearby::CredentialStorageImpl>
      credential_storage_ptr_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_MANAGER_IMPL_H_
