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

#include "internal/platform/credential_storage.h"

#include <tuple>

namespace location {
namespace nearby {

using ::nearby::internal::PrivateCredential;
using ::nearby::internal::PublicCredential;

void CredentialStorage::SaveCredentials(
    absl::string_view manager_app_id, absl::string_view account_name,
    const std::vector<PrivateCredential>& private_credentials,
    const std::vector<PublicCredential>& public_credentials,
    api::PublicCredentialType public_credential_type,
    api::SaveCredentialsResultCallback callback) {
  return impl_->SaveCredentials(manager_app_id, account_name,
                                private_credentials, public_credentials,
                                public_credential_type, callback);
}

void CredentialStorage::GetPrivateCredentials(
    const api::CredentialSelector& credential_selector,
    api::GetPrivateCredentialsResultCallback callback) {
  return impl_->GetPrivateCredentials(credential_selector, callback);
}

void CredentialStorage::GetPublicCredentials(
    const api::CredentialSelector& credential_selector,
    api::PublicCredentialType public_credential_type,
    api::GetPublicCredentialsResultCallback callback) {
  return impl_->GetPublicCredentials(credential_selector,
                                     public_credential_type, callback);
}

std::vector<::nearby::internal::PrivateCredential>
CredentialStorage::GetPrivateCredentialsForTesting(
    const std::tuple<absl::string_view, absl::string_view>& key) {
  return impl_->GetPrivateCredentialsForTesting(key);
}
std::vector<::nearby::internal::PublicCredential>
CredentialStorage::GetPublicCredentialsForTesting(
    const std::tuple<absl::string_view, absl::string_view,
                     api::PublicCredentialType>& key) {
  return impl_->GetPublicCredentialsForTesting(key);
}

}  // namespace nearby
}  // namespace location
