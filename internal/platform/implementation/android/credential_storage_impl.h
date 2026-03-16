// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_ANDROID_CREDENTIAL_STORAGE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_ANDROID_CREDENTIAL_STORAGE_H_

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

namespace nearby {
namespace android {

/*
 * The instance of CredentialStorageImpl is owned by {@code CredentialManager}.
 * It's a wrapper on top of implementation/credential_storage.h to providing
 * credential storage operations for Nearby logic layer to invoke.
 */
class CredentialStorageImpl : public api::CredentialStorage {
 public:
  ~CredentialStorageImpl() override = default;

  void SaveCredentials(absl::string_view manager_app_id,
                       absl::string_view account_name,
                       const std::vector<LocalCredential>& private_credentials,
                       const std::vector<SharedCredential>& public_credentials,
                       PublicCredentialType public_credential_type,
                       SaveCredentialsResultCallback callback) override;

  void UpdateLocalCredential(absl::string_view manager_app_id,
                             absl::string_view account_name,
                             nearby::internal::LocalCredential credential,
                             SaveCredentialsResultCallback callback) override;

  void GetLocalCredentials(const CredentialSelector& credential_selector,
                           GetLocalCredentialsResultCallback callback) override;

  void GetPublicCredentials(
      const CredentialSelector& credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback) override;
};

}  // namespace android
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_ANDROID_CREDENTIAL_STORAGE_H_
