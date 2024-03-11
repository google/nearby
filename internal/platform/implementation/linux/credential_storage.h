// Copyright 2023 Google LLC
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

#ifndef PLATFORM_IMPL_LINUX_CREDENTIAL_STORAGE_H_
#define PLATFORM_IMPL_LINUX_CREDENTIAL_STORAGE_H_
#include <memory>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/sdbus-c++.h>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/credential_storage.h"

namespace nearby {
namespace linux {
class CredentialStorage : public api::CredentialStorage {
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

  CredentialStorage(sdbus::IConnection &connection);
  ~CredentialStorage() override = default;

  void SaveCredentials(absl::string_view manager_app_id,
                       absl::string_view account_name,
                       const std::vector<LocalCredential> &Local_credentials,
                       const std::vector<SharedCredential> &Shared_credentials,
                       PublicCredentialType public_credential_type,
                       SaveCredentialsResultCallback callback) override;
  void UpdateLocalCredential(absl::string_view manager_app_id,
                             absl::string_view account_name,
                             nearby::internal::LocalCredential credential,
                             SaveCredentialsResultCallback callback) override;
  void GetPublicCredentials(
      const CredentialSelector &credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback) override;

 private:
  std::unique_ptr<sdbus::IProxy> proxy;
};
}  // namespace linux
}  // namespace nearby

#endif
