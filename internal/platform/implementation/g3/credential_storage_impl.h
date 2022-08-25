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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_G3_CREDENTIAL_STORAGE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_G3_CREDENTIAL_STORAGE_H_

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/credential_storage.h"
#include "internal/proto/credential.proto.h"

namespace location {
namespace nearby {
namespace g3 {

/*
 * The instance of CredentialStorageImpl is owned by {@code CredentialManager}.
 * It's a wrapper on top of implementation/credential_storage.h to providing
 * credential storage operations for Nearby logic layer to invoke.
 */
class CredentialStorageImpl : public api::CredentialStorage {
 public:
  explicit CredentialStorageImpl() = default;
  ~CredentialStorageImpl() override = default;

  void SaveCredentials(absl::string_view manager_app_id,
                       absl::string_view account_name,
                       const std::vector<::nearby::internal::PrivateCredential>&
                           private_credentials,
                       const std::vector<::nearby::internal::PublicCredential>&
                           public_credentials,
                       api::PublicCredentialType public_credential_type,
                       api::SaveCredentialsResultCallback callback) override;

  void SavePrivateCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<::nearby::internal::PrivateCredential>&
          private_credentials,
      api::SaveCredentialsResultCallback callback);

  void SavePublicCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<::nearby::internal::PublicCredential>&
          public_credentials,
      api::PublicCredentialType public_credential_type,
      api::SaveCredentialsResultCallback callback);

  // Used to fetch private creds when broadcasting.
  void GetPrivateCredentials(
      const api::CredentialSelector& credential_selector,
      api::GetPrivateCredentialsResultCallback callback) override;

  // Used to fetch remote public creds when scanning.
  void GetPublicCredentials(
      const api::CredentialSelector& credential_selector,
      api::PublicCredentialType public_credential_type,
      api::GetPublicCredentialsResultCallback callback) override;

 private:
  absl::flat_hash_map<std::pair<absl::string_view, absl::string_view>,
                      std::vector<::nearby::internal::PrivateCredential>>
      private_credentials_map_;
  absl::flat_hash_map<std::tuple<absl::string_view, absl::string_view,
                                 api::PublicCredentialType>,
                      std::vector<::nearby::internal::PublicCredential>>
      public_credentials_map_;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_G3_CREDENTIAL_STORAGE_H_
