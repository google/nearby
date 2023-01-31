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
#include "internal/proto/metadata.pb.h"

namespace nearby {
namespace presence {

using SubscriberId = uint64_t;

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
      const nearby::internal::Metadata& metadata,
      absl::string_view manager_app_id,
      const std::vector<nearby::internal::IdentityType>& identity_types,
      int credential_life_cycle_days, int contiguous_copy_of_credentials,
      GenerateCredentialsResultCallback credentials_generated_cb) = 0;

  // Update remote public credentials.
  virtual void UpdateRemotePublicCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<nearby::internal::SharedCredential>&
          remote_public_creds,
      UpdateRemotePublicCredentialsCallback credentials_updated_cb) = 0;

  virtual void UpdateLocalCredential(
      const CredentialSelector& credential_selector,
      nearby::internal::LocalCredential credential,
      SaveCredentialsResultCallback result_callback) = 0;

  // Used to fetch private creds when broadcasting.
  virtual void GetLocalCredentials(
      const CredentialSelector& credential_selector,
      GetLocalCredentialsResultCallback callback) = 0;

  // Used to fetch remote public creds when scanning.
  virtual void GetPublicCredentials(
      const CredentialSelector& credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback) = 0;

  // Subscribes for public credentials updates. The `callback` is triggered when
  // the public credentials are fetched initially, and then every time the
  // credentials change.
  virtual SubscriberId SubscribeForPublicCredentials(
      const CredentialSelector& credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback) = 0;

  // Unsubscribes from public credentials updates. No new callbacks will be
  // triggered after this function returns. If there is a callback already
  // running, that callback may continue after
  // `UnsubscribeFromPublicCredentials()` return.
  virtual void UnsubscribeFromPublicCredentials(SubscriberId id) = 0;

  // Decrypts the device metadata from a public credential.
  // Returns an empty string if decryption fails.
  virtual std::string DecryptMetadata(absl::string_view metadata_encryption_key,
                                      absl::string_view key_seed,
                                      absl::string_view metadata_string) = 0;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_MANAGER_H_
