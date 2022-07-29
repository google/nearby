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

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/credential_storage.h"
#include "third_party/nearby/presence/credential.h"
#include "third_party/nearby/presence/implementation/credential_manager.h"
#include "third_party/nearby/presence/presence_identity.h"

namespace nearby {
namespace presence {

class CredentialManagerImpl : public CredentialManager {
 public:
  CredentialManagerImpl() = default;

  void GenerateCredentials(
      proto::DeviceMetadata device_metadata,
      std::vector<PresenceIdentity::IdentityType> identity_types,
      GenerateCredentialsCallback credentials_generated_cb) override {}

  void UpdateRemotePublicCredentials(
      std::string account_name,
      std::vector<PublicCredential> remote_public_creds,
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

  absl::StatusOr<std::string> DecryptDataElements(
      absl::string_view metadata_key, absl::string_view salt,
      absl::string_view data_elements) override {
    return absl::UnimplementedError("DecryptDataElements unimplemented");
  }

 private:
  location::nearby::CredentialStorage*
      credential_storage_;  // NOLINT: further impl will use it.
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_MANAGER_IMPL_H_
