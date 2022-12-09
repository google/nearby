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
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/implementation/credential_storage.h"
#include "internal/proto/credential.pb.h"

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
  using PrivateCredential = ::nearby::internal::PrivateCredential;
  using PublicCredential = ::nearby::internal::PublicCredential;
  using PublicCredentialType = ::nearby::presence::PublicCredentialType;
  using PrivateCredentialKey = std::pair<std::string, std::string>;
  using PublicCredentialKey =
      std::tuple<std::string, std::string, PublicCredentialType>;

  explicit CredentialStorageImpl() = default;
  ~CredentialStorageImpl() override = default;

  // Used to save private and public credentials.
  void SaveCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<PrivateCredential>& private_credentials,
      const std::vector<PublicCredential>& public_credentials,
      PublicCredentialType public_credential_type,
      SaveCredentialsResultCallback callback) override;

  // Used to fetch private creds when broadcasting.
  void GetPrivateCredentials(
      const CredentialSelector& credential_selector,
      GetPrivateCredentialsResultCallback callback) override;

  // Used to fetch remote public creds when scanning.
  void GetPublicCredentials(
      const CredentialSelector& credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback) override;

 private:
  PrivateCredentialKey CreatePrivateCredentialKey(
      absl::string_view manager_app_id, absl::string_view account_name) {
    return std::make_tuple(std::string(manager_app_id),
                           std::string(account_name));
  }
  PublicCredentialKey CreatePublicCredentialKey(
      absl::string_view manager_app_id, absl::string_view account_name,
      PublicCredentialType credential_type) {
    return std::make_tuple(std::string(manager_app_id),
                           std::string(account_name), credential_type);
  }
  absl::flat_hash_map<PrivateCredentialKey, std::vector<PrivateCredential>>
      private_credentials_map_;
  absl::flat_hash_map<PublicCredentialKey, std::vector<PublicCredential>>
      public_credentials_map_;
  absl::Mutex private_mutex_;
  absl::Mutex public_mutex_;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_G3_CREDENTIAL_STORAGE_H_
