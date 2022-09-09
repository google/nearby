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

#include <tuple>
#include <utility>
#include <vector>

#include "internal/platform/logging.h"
#include "internal/proto/credential.proto.h"

namespace location {
namespace nearby {
namespace g3 {

using ::nearby::internal::PrivateCredential;
using ::nearby::internal::PublicCredential;

void CredentialStorageImpl::SaveCredentials(
    absl::string_view manager_app_id, absl::string_view account_name,
    const std::vector<PrivateCredential>& private_credentials,
    const std::vector<PublicCredential>& public_credentials,
    api::PublicCredentialType public_credential_type,
    api::SaveCredentialsResultCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Save Credentials for account:[ " << account_name
                    << "], Manager App ID:[" << manager_app_id << "]";
  auto save_public_creds_lambda =
      [&callback](api::CredentialOperationStatus status) {
        if (status == api::CredentialOperationStatus::kSucceeded) {
          NEARBY_LOGS(INFO) << "Saving Public Credentials succeeded!";
        } else if (status == api::CredentialOperationStatus::kFailed) {
          NEARBY_LOGS(WARNING) << "Saving Public Credentials failed!";
        } else {
          NEARBY_LOGS(WARNING) << "Saving Public Credentials status unknown!";
        }
        callback.credentials_saved_cb(status);
      };
  api::SaveCredentialsResultCallback save_public_creds_cb;
  save_public_creds_cb.credentials_saved_cb = save_public_creds_lambda;
  auto save_private_creds_lambda = [manager_app_id, account_name,
                                    &public_credentials, public_credential_type,
                                    &save_public_creds_cb, &callback, this](
                                       api::CredentialOperationStatus status) {
    if (status == api::CredentialOperationStatus::kSucceeded) {
      NEARBY_LOGS(INFO) << "Saving Private Credentials succeeded!";
      SavePublicCredentials(manager_app_id, account_name, public_credentials,
                            public_credential_type, save_public_creds_cb);
    } else if (status == api::CredentialOperationStatus::kFailed) {
      NEARBY_LOGS(WARNING) << "Saving Private Credentials failed!";
      callback.credentials_saved_cb(status);
    } else {
      NEARBY_LOGS(WARNING) << "Saving Private Credentials status unknown!";
      callback.credentials_saved_cb(status);
    }
  };
  api::SaveCredentialsResultCallback save_private_creds_cb;
  save_private_creds_cb.credentials_saved_cb = save_private_creds_lambda;

  // Invoke SavePrivateCredentials to kick off chain of credential storage.
  SavePrivateCredentials(manager_app_id, account_name, private_credentials,
                         save_private_creds_cb);
}

void CredentialStorageImpl::SavePrivateCredentials(
    absl::string_view manager_app_id, absl::string_view account_name,
    const std::vector<PrivateCredential>& private_credentials,
    api::SaveCredentialsResultCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Save Private Credentials for account: "
                    << account_name << "], manager app ID:[" << manager_app_id
                    << "]";
  auto key_value = std::make_pair(std::make_pair(manager_app_id, account_name),
                                  private_credentials);
  auto res = private_credentials_map_.insert(key_value);
  if (!res.second) {
    NEARBY_LOGS(WARNING)
        << "Credentials already saved in map. Overriding previous creds!";
    private_credentials_map_[std::make_pair(manager_app_id, account_name)] =
        private_credentials;
  }
  callback.credentials_saved_cb(api::CredentialOperationStatus::kSucceeded);
}

void CredentialStorageImpl::SavePublicCredentials(
    absl::string_view manager_app_id, absl::string_view account_name,
    const std::vector<PublicCredential>& public_credentials,
    api::PublicCredentialType public_credential_type,
    api::SaveCredentialsResultCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Save Public Credentials for account: "
                    << account_name << "], manager app ID:[" << manager_app_id
                    << "]";
  auto key_value = std::make_pair(
      std::make_tuple(manager_app_id, account_name, public_credential_type),
      public_credentials);
  auto res = public_credentials_map_.insert(key_value);
  if (!res.second) {
    NEARBY_LOGS(WARNING)
        << "Credentials already saved in map. Overriding previous creds!";
    public_credentials_map_[std::make_tuple(manager_app_id, account_name,
                                            public_credential_type)] =
        public_credentials;
  }
  callback.credentials_saved_cb(api::CredentialOperationStatus::kSucceeded);
}

void CredentialStorageImpl::GetPrivateCredentials(
    const api::CredentialSelector& credential_selector,
    api::GetPrivateCredentialsResultCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Get Private Credentials for account:[ "
                    << credential_selector.account_name << "], manager app ID:["
                    << credential_selector.manager_app_id << "]";
  auto key = std::make_pair(credential_selector.manager_app_id,
                            credential_selector.account_name);
  if (private_credentials_map_.find(key) == private_credentials_map_.end()) {
    NEARBY_LOGS(WARNING) << "There are no Private Credentials stored for key:"
                         << std::get<0>(key) << ", " << std::get<1>(key);
    callback.get_credentials_failed_cb(api::CredentialOperationStatus::kFailed);
  } else {
    std::vector<PrivateCredential> private_credentials =
        private_credentials_map_[key];
    callback.credentials_fetched_cb(private_credentials);
  }
}

void CredentialStorageImpl::GetPublicCredentials(
    const api::CredentialSelector& credential_selector,
    api::PublicCredentialType public_credential_type,
    api::GetPublicCredentialsResultCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Get Public Credentials for account: "
                    << credential_selector.account_name;
  auto key =
      std::make_tuple(credential_selector.manager_app_id,
                      credential_selector.account_name, public_credential_type);
  if (public_credentials_map_.find(key) == public_credentials_map_.end()) {
    NEARBY_LOGS(WARNING) << "There are no Public Credentials stored for key:"
                         << std::get<0>(key) << ", " << std::get<1>(key) << ", "
                         << std::get<2>(key);
    callback.get_credentials_failed_cb(api::CredentialOperationStatus::kFailed);
  } else {
    std::vector<PublicCredential> public_credentials =
        public_credentials_map_[key];
    callback.credentials_fetched_cb(public_credentials);
  }
}
}  // namespace g3
}  // namespace nearby
}  // namespace location
