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
#include "internal/platform/credential_storage_impl.h"

#include <utility>
#include <vector>

#include "internal/platform/implementation/credential_callbacks.h"

namespace nearby {

using ::nearby::internal::LocalCredential;
using ::nearby::internal::SharedCredential;
using ::nearby::presence::CredentialSelector;
using ::nearby::presence::GetPublicCredentialsResultCallback;
using ::nearby::presence::PublicCredentialType;

void CredentialStorageImpl::SaveCredentials(
    absl::string_view manager_app_id, absl::string_view account_name,
    const std::vector<LocalCredential>& private_credentials,
    const std::vector<SharedCredential>& public_credentials,
    PublicCredentialType public_credential_type,
    SaveCredentialsResultCallback callback) {
  return impl_->SaveCredentials(manager_app_id, account_name,
                                private_credentials, public_credentials,
                                public_credential_type, std::move(callback));
}

void CredentialStorageImpl::UpdateLocalCredential(
    absl::string_view manager_app_id, absl::string_view account_name,
    nearby::internal::LocalCredential credential,
    SaveCredentialsResultCallback callback) {
  return impl_->UpdateLocalCredential(
      manager_app_id, account_name, std::move(credential), std::move(callback));
}

void CredentialStorageImpl::GetLocalCredentials(
    const CredentialSelector& credential_selector,
    GetLocalCredentialsResultCallback callback) {
  return impl_->GetLocalCredentials(credential_selector, std::move(callback));
}

void CredentialStorageImpl::GetPublicCredentials(
    const CredentialSelector& credential_selector,
    PublicCredentialType public_credential_type,
    GetPublicCredentialsResultCallback callback) {
  return impl_->GetPublicCredentials(
      credential_selector, public_credential_type, std::move(callback));
}

}  // namespace nearby
