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

#include "internal/platform/implementation/g3/credential_storage_impl.h"

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "internal/platform/logging.h"
#include "internal/proto/credential.pb.h"

namespace nearby {
namespace g3 {

namespace {
using ::nearby::internal::IdentityType;

// Removes credentials (public or private) that don't match `identity_type` from
// the collection.
template <class Credential>
void FilterIdentityType(std::vector<Credential>& credentials,
                        IdentityType identity_type) {
  if (identity_type == IdentityType::IDENTITY_TYPE_UNSPECIFIED) {
    return;
  }
  auto it = credentials.begin();
  while (it != credentials.end()) {
    if (it->identity_type() != identity_type) {
      it = credentials.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace

void CredentialStorageImpl::SaveCredentials(
    absl::string_view manager_app_id, absl::string_view account_name,
    const std::vector<LocalCredential>& private_credentials,
    const std::vector<SharedCredential>& public_credentials,
    PublicCredentialType public_credential_type,
    SaveCredentialsResultCallback callback) {
  if (private_credentials.empty() && public_credentials.empty()) {
    std::move(callback.credentials_saved_cb)(
        absl::InvalidArgumentError("No credentials to save"));
    return;
  }
  if (private_credentials.empty()) {
    NEARBY_LOGS(INFO) << "There are no Private Credentials for account: ["
                      << account_name << "], manager app ID:[" << manager_app_id
                      << "]";
  } else {
    NEARBY_LOGS(INFO) << "G3 Save Private Credentials for account: ["
                      << account_name << "], manager app ID:[" << manager_app_id
                      << "]";
    absl::MutexLock lock(&private_mutex_);
    SaveLocalCredentialsLocked(manager_app_id, account_name,
                               private_credentials);
  }

  if (public_credentials.empty()) {
    NEARBY_LOGS(INFO) << "There are no Public Credentials for account: ["
                      << account_name << "], manager app ID:[" << manager_app_id
                      << "]";
  } else {
    NEARBY_LOGS(INFO) << "G3 Save Public Credentials for account: ["
                      << account_name << "], manager app ID:[" << manager_app_id
                      << "]";
    absl::MutexLock lock(&public_mutex_);
    PublicCredentialKey key = CreatePublicCredentialKey(
        manager_app_id, account_name, public_credential_type);
    auto public_result =
        public_credentials_map_.insert(std::make_pair(key, public_credentials));
    if (!public_result.second) {
      NEARBY_LOGS(WARNING)
          << "Credentials already saved in map. Overwriting previous creds!";
      public_credentials_map_[key] = public_credentials;
    }
  }
  std::move(callback.credentials_saved_cb)(absl::OkStatus());
}
void CredentialStorageImpl::SaveLocalCredentialsLocked(
    absl::string_view manager_app_id, absl::string_view account_name,
    const std::vector<LocalCredential>& private_credentials) {
  LocalCredentialKey key =
      CreateLocalCredentialKey(manager_app_id, account_name);
  auto private_result =
      private_credentials_map_.insert(std::make_pair(key, private_credentials));
  if (!private_result.second) {
    NEARBY_LOGS(WARNING)
        << "Credentials already saved in map. Overwriting previous creds!";
    private_credentials_map_[key] = private_credentials;
  }
}

void CredentialStorageImpl::UpdateLocalCredential(
    absl::string_view manager_app_id, absl::string_view account_name,
    LocalCredential credential, SaveCredentialsResultCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Update Private Credential for for account: ["
                    << account_name << "], manager app ID:[" << manager_app_id
                    << "]";
  absl::MutexLock lock(&private_mutex_);
  absl::StatusOr<std::vector<LocalCredential>> credentials =
      GetLocalCredentialsLocked(CredentialSelector{
          .manager_app_id = std::string(manager_app_id),
          .account_name = std::string(account_name),
          .identity_type = IdentityType::IDENTITY_TYPE_UNSPECIFIED});
  if (!credentials.ok()) {
    NEARBY_LOGS(WARNING) << credentials.status();
    credentials = std::vector<LocalCredential>();
  }
  auto it = std::find_if(credentials->begin(), credentials->end(),
                         [&](const LocalCredential& a) {
                           return a.secret_id() == credential.secret_id();
                         });
  if (it == credentials->end()) {
    credentials->push_back(std::move(credential));
  } else {
    *it = std::move(credential);
  }
  SaveLocalCredentialsLocked(manager_app_id, account_name, *credentials);
  callback.credentials_saved_cb(absl::OkStatus());
}

void CredentialStorageImpl::GetLocalCredentials(
    const CredentialSelector& credential_selector,
    GetLocalCredentialsResultCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Get Private Credentials for " << credential_selector;
  absl::MutexLock lock(&private_mutex_);
  std::move(callback.credentials_fetched_cb)(
      GetLocalCredentialsLocked(credential_selector));
}

absl::StatusOr<std::vector<nearby::internal::LocalCredential>>
CredentialStorageImpl::GetLocalCredentialsLocked(
    const CredentialSelector& credential_selector) {
  LocalCredentialKey key = CreateLocalCredentialKey(
      credential_selector.manager_app_id, credential_selector.account_name);
  if (private_credentials_map_.find(key) == private_credentials_map_.end()) {
    NEARBY_LOGS(WARNING) << "There are no Private Credentials stored for key:"
                         << std::get<0>(key) << ", " << std::get<1>(key);
    return absl::NotFoundError(
        absl::StrFormat("No private credentials for %v", credential_selector));
  }
  std::vector<LocalCredential> private_credentials =
      private_credentials_map_[key];
  FilterIdentityType(private_credentials, credential_selector.identity_type);
  if (private_credentials.empty()) {
    return absl::NotFoundError(
        absl::StrFormat("No private credentials for %v", credential_selector));
  }
  return private_credentials;
}

void CredentialStorageImpl::GetPublicCredentials(
    const CredentialSelector& credential_selector,
    PublicCredentialType public_credential_type,
    GetPublicCredentialsResultCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Get Public Credentials for " << credential_selector;
  absl::MutexLock lock(&public_mutex_);
  PublicCredentialKey key = CreatePublicCredentialKey(
      credential_selector.manager_app_id, credential_selector.account_name,
      public_credential_type);
  if (public_credentials_map_.find(key) == public_credentials_map_.end()) {
    NEARBY_LOGS(WARNING) << "There are no Public Credentials stored for key:"
                         << std::get<0>(key) << ", " << std::get<1>(key) << ", "
                         << std::get<2>(key);
    std::move(callback.credentials_fetched_cb)(absl::NotFoundError(
        absl::StrFormat("No public credentials for %v", credential_selector)));
    return;
  }
  std::vector<SharedCredential> public_credentials =
      public_credentials_map_[key];
  FilterIdentityType(public_credentials, credential_selector.identity_type);
  if (public_credentials.empty()) {
    std::move(callback.credentials_fetched_cb)(absl::NotFoundError(
        absl::StrFormat("No public credentials for %v", credential_selector)));
    return;
  }
  std::move(callback.credentials_fetched_cb)(public_credentials);
}

}  // namespace g3
}  // namespace nearby
