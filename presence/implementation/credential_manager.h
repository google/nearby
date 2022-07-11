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

#include "internal/platform/credential.h"
#include "internal/platform/credential_storage.h"

namespace nearby {
namespace presence {

struct GenerateCredentialsCallback {
  std::function<void(std::vector<location::nearby::PublicCredential>)>
      credentials_generated_cb;
};

struct UpdateRemotePublicCredentialsCallback {
  std::function<void(location::nearby::api::CredentialOperationStatus)>
      credentials_updated_cb;
};

/*
 * The instance of CredentialManager is owned by {@code ServiceControllerImpl}.
 * Helping service controller to manage local credentials and coordinate with
 * downloaded remote credentials.
 */
class CredentialManager {
 public:
  CredentialManager() = default;
  ~CredentialManager() = default;

  // Used to (re)generate user’s private and public credentials.
  // The generated private credentials will be saved to creds storage.
  // The generated public credentials will be returned inside the
  // credentials_generated_cb for manager app to upload to web.
  // The user’s own public credentials won’t be saved on local credential
  // storage.
  void GenerateCredentials(
      location::nearby::DeviceMetadata device_metadata,
      std::vector<location::nearby::TrustType> trust_types,
      GenerateCredentialsCallback credentials_generated_cb);

  // Update remote public credentials.
  void UpdateRemotePublicCredentials(
      std::string account_name,
      std::vector<location::nearby::PublicCredential> remote_public_creds,
      UpdateRemotePublicCredentialsCallback credentials_updated_cb);

  // Used to fetch private creds when broadcasting.
  void GetPrivateCredentials(
      location::nearby::api::CredentialSelector credential_selector,
      location::nearby::api::GetPrivateCredentialCallback callback);

  // Used to fetch remote public creds when scanning.
  void GetPublicCredentials(
      location::nearby::api::CredentialSelector credential_selector,
      location::nearby::api::GetPublicCredentialCallback callback);

 private:
  location::nearby::CredentialStorage* credential_storage_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_MANAGER_H_
