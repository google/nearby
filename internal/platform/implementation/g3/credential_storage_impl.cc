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

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "internal/platform/logging.h"
#include "internal/proto/credential.pb.h"

namespace location {
namespace nearby {
namespace g3 {

using ::nearby::internal::PrivateCredential;
using ::nearby::internal::PublicCredential;

void CredentialStorageImpl::SaveCredentials(
    absl::string_view manager_app_id, absl::string_view account_name,
    const std::vector<PrivateCredential>& private_credentials,
    const std::vector<PublicCredential>& public_credentials,
    PublicCredentialType public_credential_type,
    SaveCredentialsResultCallback callback) {
  if (private_credentials.empty() && public_credentials.empty()) {
    NEARBY_LOGS(INFO) << "G3 Save Credentials but seeing private and public "
                         "both empty, skipping";
    return;
  }
  if (private_credentials.empty()) {
    NEARBY_LOGS(INFO) << "There are no Private Credentials for account: "
                      << account_name << "], manager app ID:[" << manager_app_id
                      << "]";
  } else {
    NEARBY_LOGS(INFO) << "G3 Save Private Credentials for account: "
                      << account_name << "], manager app ID:[" << manager_app_id
                      << "]";
    absl::MutexLock lock(&private_mutex_);
    PrivateCredentialKey key =
        CreatePrivateCredentialKey(manager_app_id, account_name);
    auto private_result = private_credentials_map_.insert(
        std::make_pair(key, private_credentials));
    if (!private_result.second) {
      NEARBY_LOGS(WARNING)
          << "Credentials already saved in map. Overwriting previous creds!";
      private_credentials_map_[key] = private_credentials;
    }
  }

  if (public_credentials.empty()) {
    NEARBY_LOGS(INFO) << "There are no Public Credentials for account: "
                      << account_name << "], manager app ID:[" << manager_app_id
                      << "]";
    return;
  } else {
    NEARBY_LOGS(INFO) << "G3 Save Public Credentials for account: "
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

void CredentialStorageImpl::GetPrivateCredentials(
    const ::nearby::presence::CredentialSelector& credential_selector,
    ::nearby::presence::GetPrivateCredentialsResultCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Get Private Credentials for " << credential_selector;
  absl::MutexLock lock(&private_mutex_);
  PrivateCredentialKey key = CreatePrivateCredentialKey(
      credential_selector.manager_app_id, credential_selector.account_name);
  if (private_credentials_map_.find(key) == private_credentials_map_.end()) {
    NEARBY_LOGS(WARNING) << "There are no Private Credentials stored for key:"
                         << std::get<0>(key) << ", " << std::get<1>(key);
    std::move(callback.get_credentials_failed_cb)(absl::NotFoundError(
        absl::StrFormat("No private credentials for %v", credential_selector)));
  } else {
    std::vector<PrivateCredential> private_credentials =
        private_credentials_map_[key];
    std::move(callback.credentials_fetched_cb)(private_credentials);
  }
}

void CredentialStorageImpl::GetPublicCredentials(
    const ::nearby::presence::CredentialSelector& credential_selector,
    ::nearby::presence::PublicCredentialType public_credential_type,
    ::nearby::presence::GetPublicCredentialsResultCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Get Public Credentials for " << credential_selector;
  absl::MutexLock lock(&public_mutex_);
  PublicCredentialKey key = CreatePublicCredentialKey(
      credential_selector.manager_app_id, credential_selector.account_name,
      public_credential_type);
  if (public_credentials_map_.find(key) == public_credentials_map_.end()) {
    NEARBY_LOGS(WARNING) << "There are no Public Credentials stored for key:"
                         << std::get<0>(key) << ", " << std::get<1>(key) << ", "
                         << std::get<2>(key);
    std::move(callback.get_credentials_failed_cb)(absl::NotFoundError(
        absl::StrFormat("No public credentials for %v", credential_selector)));
  } else {
    std::vector<PublicCredential> public_credentials =
        public_credentials_map_[key];
    std::move(callback.credentials_fetched_cb)(public_credentials);
  }
}
}  // namespace g3
}  // namespace nearby
}  // namespace location
