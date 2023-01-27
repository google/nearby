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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_CREDENTIAL_STORAGE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_CREDENTIAL_STORAGE_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/proto/credential.pb.h"

namespace nearby {
namespace api {

// Credential Storage interface
class CredentialStorage {
 public:
  using LocalCredential = ::nearby::internal::LocalCredential;
  using SharedCredential = ::nearby::internal::SharedCredential;
  using PublicCredentialType = ::nearby::presence::PublicCredentialType;
  using SaveCredentialsResultCallback =
      ::nearby::presence::SaveCredentialsResultCallback;
  using CredentialSelector = ::nearby::presence::CredentialSelector;
  using GetLocalCredentialsResultCallback =
      ::nearby::presence::GetLocalCredentialsResultCallback;
  using GetPublicCredentialsResultCallback =
      ::nearby::presence::GetPublicCredentialsResultCallback;

  virtual ~CredentialStorage() = default;

  // Saves the credentials in the storage.
  //
  // If `private_credentials` is not empty, then the private credentials in the
  // storage, associated with `manager_app_id`/`account_name` pair, are replaced
  // with given credentials.
  //
  // If `public_credentials` is not empty, then the public credentials of
  // `public_credential_type` type in the storage, associated with
  // `manager_app_id`/`account_name` pair, are replaced with given credentials.
  //
  // Note, both private and public credentials have a `identity_type` field,
  // which is used for querying credentials.
  virtual void SaveCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<LocalCredential>& Local_credentials,
      const std::vector<SharedCredential>& Shared_credentials,
      PublicCredentialType public_credential_type,
      SaveCredentialsResultCallback callback) = 0;

  // Updates the `credential` in the storage. LocalCredential has a
  // `secret_id` field, which uniquely identifies the credential.
  virtual void UpdateLocalCredential(
      absl::string_view manager_app_id, absl::string_view account_name,
      nearby::internal::LocalCredential credential,
      SaveCredentialsResultCallback callback) = 0;

  // Fetches private credentials.
  //
  // When `credential_selector.identity_type` is not set (unspecified), then
  // private credentials with any identity type should be returned.
  virtual void GetLocalCredentials(
      const CredentialSelector& credential_selector,
      GetLocalCredentialsResultCallback callback) = 0;

  // Fetches public credentials.
  //
  // When `credential_selector.identity_type` is not set (unspecified), then
  // public credentials with any identity type should be returned.
  virtual void GetPublicCredentials(
      const CredentialSelector& credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_CREDENTIAL_STORAGE_H_
