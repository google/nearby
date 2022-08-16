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
#include "internal/platform/credential_storage.h"
#include "internal/proto/credential.pb.h"
#include "presence/implementation/credential_manager.h"

namespace nearby {
namespace presence {

class CredentialManagerImpl : public CredentialManager {
 public:
  CredentialManagerImpl() = default;

  // AES only supports key sizes of 16, 24 or 32 bytes.
  static constexpr int kAuthenticityKeyByteSize = 16;

  // Length of key in bytes required by AES-GCM encryption.
  static constexpr size_t kNearbyPresenceNumBytesAesGcmKeySize = 32;

  // Modify this to 12 after use real AES.
  static constexpr int kAesGcmIVSize = 12;

  void GenerateCredentials(
      nearby::internal::DeviceMetadata device_metadata,
      std::string manager_app_id,
      std::vector<nearby::internal::IdentityType> identity_types,
      int credential_life_cycle_days,
      int contiguous_copy_of_credentials,
      GenerateCredentialsCallback credentials_generated_cb) override;

  void UpdateRemotePublicCredentials(
      std::string manager_app_id, std::string account_name,
      std::vector<nearby::internal::PublicCredential> remote_public_creds,
      UpdateRemotePublicCredentialsCallback credentials_updated_cb) override{};

  void GetPrivateCredentials(
      location::nearby::api::CredentialSelector credential_selector,
      location::nearby::api::GetPrivateCredentialsResultCallback callback)
      override {}

  // Used to fetch remote public creds when scanning.
  void GetPublicCredentials(
      location::nearby::api::CredentialSelector credential_selector,
      location::nearby::api::GetPublicCredentialsResultCallback callback)
      override {}

  std::string DecryptDeviceMetadata(
      std::string device_metadata_encryption_key, std::string authenticity_key,
      std::string device_metadata_string) override;

  absl::StatusOr<std::string> DecryptDataElements(
      absl::string_view metadata_key, absl::string_view salt,
      absl::string_view data_elements) override {
    return absl::UnimplementedError("DecryptDataElements unimplemented");
  }

  absl::StatusOr<std::string> GetBaseEncryptedMetadataKey(
      const nearby::internal::IdentityType& identity) override {
    return absl::UnimplementedError(
        "GetBaseEncryptedMetadataKey unimplemented");
  }

  absl::StatusOr<std::string> EncryptDataElements(
      const nearby::internal::IdentityType& identity, absl::string_view salt,
      absl::string_view data_elements) override {
    return absl::UnimplementedError("EncryptDataElements unimplemented");
  }

 private:
  FRIEND_TEST(CredentialManagerImpl, CreateOneCredentialSuccessfully);

  std::pair<std::unique_ptr<nearby::internal::PrivateCredential>,
            std::unique_ptr<nearby::internal::PublicCredential>>
  CreatePrivateCredential(nearby::internal::DeviceMetadata device_metadata,
                          nearby::internal::IdentityType identity_type,
                          uint64_t start_time_ms, uint64_t end_time_ms);

  std::unique_ptr<nearby::internal::PublicCredential> CreatePublicCredential(
      nearby::internal::PrivateCredential* private_credential_ptr,
      std::vector<uint8_t>* public_key);

  std::string EncryptDeviceMetadata(std::string device_metadata_encryption_key,
                                    std::string authenticity_key,
                                    std::string device_metadata_string);

  // Extend the key from 16 bytes to 32 bytes.
  std::vector<uint8_t> ExtendMetadataEncryptionKey(
      std::string device_metadata_encryption_key);
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_MANAGER_IMPL_H_
