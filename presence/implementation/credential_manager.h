// Copyright 2020 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_MANAGER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_MANAGER_H_

#include <functional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/proto/credential.pb.h"

namespace nearby {
namespace presence {

/*
 * The instance of CredentialManager is owned by {@code ServiceControllerImpl}.
 * Helping service controller to manage local credentials and coordinate with
 * downloaded remote credentials.
 */
class CredentialManager {
 public:
  CredentialManager() = default;
  virtual ~CredentialManager() = default;

  // Used to (re)generate user’s private and public credentials.
  // The generated private credentials will be saved to creds storage.
  // The generated public credentials will be returned inside the
  // credentials_generated_cb for manager app to upload to web.
  // The user’s own public credentials won’t be saved on local credential
  // storage.
  virtual void GenerateCredentials(
      const nearby::internal::DeviceMetadata& device_metadata,
      absl::string_view manager_app_id,
      const std::vector<nearby::internal::IdentityType>& identity_types,
      int credential_life_cycle_days, int contiguous_copy_of_credentials,
      GenerateCredentialsCallback credentials_generated_cb) = 0;

  // Update remote public credentials.
  virtual void UpdateRemotePublicCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<nearby::internal::PublicCredential>&
          remote_public_creds,
      UpdateRemotePublicCredentialsCallback credentials_updated_cb) = 0;

  // Used to fetch private creds when broadcasting.
  virtual void GetPrivateCredentials(
      const CredentialSelector& credential_selector,
      GetPrivateCredentialsResultCallback callback) = 0;

  // Used to fetch remote public creds when scanning.
  virtual void GetPublicCredentials(
      const CredentialSelector& credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback) = 0;

  // Decrypts the device metadata from a public credential.
  // Returns an empty string if decryption fails.
  virtual std::string DecryptDeviceMetadata(
      absl::string_view device_metadata_encryption_key,
      absl::string_view authenticity_key,
      absl::string_view device_metadata_string) = 0;

  // Decrypts Data Elements from an NP advertisement.
  // Returns an error if `data_elements` could not be deciphered with any known
  // credentials (identity).
  virtual absl::StatusOr<std::string> DecryptDataElements(
      absl::string_view salt, absl::string_view data_elements) = 0;

  // Encrypts `data_elements` using certificate associated with `identity` and
  // `salt`.
  virtual absl::StatusOr<std::string> EncryptDataElements(
      nearby::internal::IdentityType identity, absl::string_view salt,
      absl::string_view data_elements) = 0;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_MANAGER_H_
