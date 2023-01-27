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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_CREDENTIAL_STORAGE_IMPL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_CREDENTIAL_STORAGE_IMPL_H_

#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/implementation/credential_storage.h"
#include "internal/platform/implementation/platform.h"

namespace nearby {

/*
 * The instance of CredentialStorageImpl is owned by {@code CredentialManager}.
 * It's a wrapper on top of implementation/credential_storage.h to providing
 * credential storage operations for Nearby logic layer to invoke.
 */
class CredentialStorageImpl : public api::CredentialStorage {
 public:
  explicit CredentialStorageImpl()
      : impl_(api::ImplementationPlatform::CreateCredentialStorage()) {}

  // CredentialStorageImpl class is movable but not copyable.
  CredentialStorageImpl(CredentialStorageImpl&& other) = default;
  CredentialStorageImpl& operator=(CredentialStorageImpl&& other) = default;

  void SaveCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<LocalCredential>& private_credentials,
      const std::vector<SharedCredential>& public_credentials,
      PublicCredentialType public_credential_type,
      SaveCredentialsResultCallback callback) override;

  void UpdateLocalCredential(absl::string_view manager_app_id,
                               absl::string_view account_name,
                               nearby::internal::LocalCredential credential,
                               SaveCredentialsResultCallback callback) override;

  // Used to fetch private creds when broadcasting.
  void GetLocalCredentials(
      const CredentialSelector& credential_selector,
      GetLocalCredentialsResultCallback callback) override;

  // Used to fetch remote public creds when scanning.
  void GetPublicCredentials(
      const CredentialSelector& credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback) override;

 private:
  std::unique_ptr<api::CredentialStorage> impl_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_CREDENTIAL_STORAGE_IMPL_H_
