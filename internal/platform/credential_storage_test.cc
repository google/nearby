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

#include "internal/platform/credential_storage.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/proto/credential.pb.h"

namespace location {
namespace nearby {

using ::nearby::internal::PrivateCredential;
using ::nearby::internal::PublicCredential;

class CredentialStorageTest : public testing::Test {
 public:
  class CredentialStoragePublic : public CredentialStorage {
   public:
    using CredentialStorage::GetPrivateCredentialsForTesting;
    using CredentialStorage::GetPublicCredentialsForTesting;
  };
};

TEST_F(CredentialStorageTest, CanSaveCredentials) {
  // Define mock parameters
  std::string manager_app_id{"0192"};
  std::string account_name{"test_account"};
  api::PublicCredentialType public_credential_type{
      api::PublicCredentialType::kLocalPublicCredential};
  // Create mock credentials
  PrivateCredential private_credential;
  PublicCredential public_credential;
  private_credential.set_secret_id("secret_id");
  public_credential.set_secret_id("secret_id");
  std::vector<PrivateCredential> private_credentials;
  std::vector<PublicCredential> public_credentials;
  private_credentials.push_back(private_credential);
  public_credentials.push_back(public_credential);
  // Define SaveCredentialsResultCallback
  bool successfull_save{false};
  auto save_creds_lambda =
      [&successfull_save](api::CredentialOperationStatus status) {
        if (status == api::CredentialOperationStatus::kSucceeded) {
          successfull_save = true;
        }
      };
  api::SaveCredentialsResultCallback save_creds_callback;
  save_creds_callback.credentials_saved_cb = save_creds_lambda;
  // Create CredentialStorage object to test SaveCredentials method
  CredentialStoragePublic creds_storage;
  creds_storage.SaveCredentials(manager_app_id, account_name,
                                private_credentials, public_credentials,
                                public_credential_type, save_creds_callback);
  EXPECT_TRUE(successfull_save);
  auto private_creds_key = std::make_tuple(manager_app_id, account_name);
  auto private_creds_test =
      creds_storage.GetPrivateCredentialsForTesting(private_creds_key);
  EXPECT_EQ(private_creds_test[0].secret_id(),
            private_credentials[0].secret_id());
  auto public_creds_key =
      std::make_tuple(manager_app_id, account_name, public_credential_type);
  auto public_creds_test =
      creds_storage.GetPublicCredentialsForTesting(public_creds_key);
  EXPECT_EQ(public_creds_test[0].secret_id(),
            public_credentials[0].secret_id());
}

TEST_F(CredentialStorageTest, CanGetPrivateCredentials) {
  // Define mock parameters
  std::string manager_app_id{"0192"};
  std::string account_name{"test_account"};
  api::CredentialSelector credential_selector;
  credential_selector.manager_app_id = manager_app_id;
  credential_selector.account_name = account_name;
  api::PublicCredentialType public_credential_type{
      api::PublicCredentialType::kLocalPublicCredential};
  // Create mock credentials
  PrivateCredential private_credential;
  PublicCredential public_credential;
  private_credential.set_secret_id("secret_id");
  public_credential.set_secret_id("secret_id");
  std::vector<PrivateCredential> private_credentials;
  std::vector<PublicCredential> public_credentials;
  private_credentials.push_back(private_credential);
  public_credentials.push_back(public_credential);
  // Define SaveCredentialsResultCallback
  auto save_creds_lambda = [](api::CredentialOperationStatus status) {};
  api::SaveCredentialsResultCallback save_creds_callback;
  save_creds_callback.credentials_saved_cb = save_creds_lambda;
  // Define GetPrivateCredentialsResultCallback
  bool get_private_cred_succeeded{false};
  auto credentials_fetched_lambda =
      [&private_credentials, &get_private_cred_succeeded](
          const std::vector<PrivateCredential> &private_creds) {
        auto private_cred = private_creds[0];
        auto private_credential = private_credentials[0];
        if (private_cred.secret_id() == private_credential.secret_id()) {
          get_private_cred_succeeded = true;
        }
      };
  auto get_credentials_failed_lambda =
      [&get_private_cred_succeeded](api::CredentialOperationStatus status) {
        if (status == api::CredentialOperationStatus::kFailed) {
          get_private_cred_succeeded = false;
        }
      };
  api::GetPrivateCredentialsResultCallback get_private_creds_callback;
  get_private_creds_callback.credentials_fetched_cb =
      credentials_fetched_lambda;
  get_private_creds_callback.get_credentials_failed_cb =
      get_credentials_failed_lambda;
  // Create CredentialStorage object to test GetPrivateCredentials
  CredentialStorage creds_storage;
  creds_storage.GetPrivateCredentials(credential_selector,
                                      get_private_creds_callback);
  EXPECT_FALSE(get_private_cred_succeeded);
  creds_storage.SaveCredentials(manager_app_id, account_name,
                                private_credentials, public_credentials,
                                public_credential_type, save_creds_callback);
  creds_storage.GetPrivateCredentials(credential_selector,
                                      get_private_creds_callback);
  EXPECT_TRUE(get_private_cred_succeeded);
}

TEST_F(CredentialStorageTest, CanGetPublicCredentials) {
  // Define mock parameters
  std::string manager_app_id{"0192"};
  std::string account_name{"test_account"};
  api::CredentialSelector credential_selector;
  credential_selector.manager_app_id = manager_app_id;
  credential_selector.account_name = account_name;
  api::PublicCredentialType public_credential_type{
      api::PublicCredentialType::kLocalPublicCredential};
  // Create mock credentials
  PrivateCredential private_credential;
  PublicCredential public_credential;
  private_credential.set_secret_id("secret_id");
  public_credential.set_secret_id("secret_id");
  std::vector<PrivateCredential> private_credentials;
  std::vector<PublicCredential> public_credentials;
  private_credentials.push_back(private_credential);
  public_credentials.push_back(public_credential);
  // Define SaveCredentialsResultCallback
  auto save_creds_lambda = [](api::CredentialOperationStatus status) {};
  api::SaveCredentialsResultCallback save_creds_callback;
  save_creds_callback.credentials_saved_cb = save_creds_lambda;
  // Define GetPublicCredentialsResultCallback
  bool get_public_cred_succeeded{false};
  auto credentials_fetched_lambda =
      [&public_credentials, &get_public_cred_succeeded](
          const std::vector<PublicCredential> &public_creds) {
        auto public_cred = public_creds[0];
        auto public_credential = public_credentials[0];
        if (public_cred.secret_id() == public_credential.secret_id()) {
          get_public_cred_succeeded = true;
        }
      };
  auto get_credentials_failed_lambda =
      [&get_public_cred_succeeded](api::CredentialOperationStatus status) {
        if (status == api::CredentialOperationStatus::kFailed) {
          get_public_cred_succeeded = false;
        }
      };
  api::GetPublicCredentialsResultCallback get_public_creds_callback;
  get_public_creds_callback.credentials_fetched_cb = credentials_fetched_lambda;
  get_public_creds_callback.get_credentials_failed_cb =
      get_credentials_failed_lambda;
  // Create CredentialStorage object to test GetPublicCredentials
  CredentialStorage creds_storage;
  creds_storage.GetPublicCredentials(
      credential_selector, public_credential_type, get_public_creds_callback);
  EXPECT_FALSE(get_public_cred_succeeded);
  creds_storage.SaveCredentials(manager_app_id, account_name,
                                private_credentials, public_credentials,
                                public_credential_type, save_creds_callback);
  creds_storage.GetPublicCredentials(
      credential_selector, public_credential_type, get_public_creds_callback);
  EXPECT_TRUE(get_public_cred_succeeded);
}

}  // namespace nearby
}  // namespace location
