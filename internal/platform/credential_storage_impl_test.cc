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

#include "internal/platform/credential_storage_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/credential.proto.h"

namespace location {
namespace nearby {
namespace {

using ::nearby::internal::PrivateCredential;
using ::nearby::internal::PublicCredential;
using ::nearby::presence::CredentialOperationStatus;
using ::nearby::presence::CredentialSelector;
using ::nearby::presence::GenerateCredentialsCallback;
using ::nearby::presence::GetPrivateCredentialsResultCallback;
using ::nearby::presence::GetPublicCredentialsResultCallback;
using ::nearby::presence::PublicCredentialType;

std::vector<PrivateCredential> BuildDefaultPrivateCreds() {
  PrivateCredential private_credential;
  private_credential.set_secret_id("secret_id");
  std::vector<PrivateCredential> private_credentials;
  private_credentials.push_back(private_credential);
  return private_credentials;
}

std::vector<PublicCredential> BuildDefaultPublicCreds() {
  PublicCredential public_credential;
  public_credential.set_secret_id("secret_id");
  std::vector<PublicCredential> public_credentials;
  public_credentials.push_back(public_credential);
  return public_credentials;
}

CredentialSelector BuildDefaultCredentialSelector() {
  constexpr absl::string_view kAppId = "0192";
  constexpr absl::string_view kAccountName = "test_account";
  CredentialSelector credential_selector;
  credential_selector.manager_app_id = std::string(kAppId);
  credential_selector.account_name = std::string(kAccountName);
  return credential_selector;
}

TEST(CredentialStorageImplTest, CanSaveAndGetPrivateCredentials) {
  std::vector<PrivateCredential> default_private_creds =
      BuildDefaultPrivateCreds();
  std::vector<PublicCredential> default_public_creds =
      BuildDefaultPublicCreds();
  std::vector<PrivateCredential> empty_private_creds;
  std::vector<PublicCredential> empty_public_creds;
  bool successfull_save = false;
  GenerateCredentialsCallback generate_creds_callback;
  generate_creds_callback.credentials_generated_cb =
      [&successfull_save, &default_public_creds](
          const std::vector<PublicCredential>& public_creds) {
        if (public_creds[0].secret_id() ==
            default_public_creds[0].secret_id()) {
          successfull_save = true;
        }
      };
  std::vector<PrivateCredential> retrieved_creds;
  GetPrivateCredentialsResultCallback get_private_creds_callback;
  get_private_creds_callback.credentials_fetched_cb =
      [&retrieved_creds](const std::vector<PrivateCredential>& private_creds) {
        retrieved_creds = private_creds;
      };
  bool get_private_cred_failed = false;
  get_private_creds_callback.get_credentials_failed_cb =
      [&get_private_cred_failed](CredentialOperationStatus status) {
        if (status == CredentialOperationStatus::kFailed) {
          get_private_cred_failed = true;
        }
      };
  // Create CredentialStorageImpl object to test SaveCredentials &
  // GetPrivateCredentials
  CredentialSelector cred_selector = BuildDefaultCredentialSelector();
  PublicCredentialType public_credential_type =
      PublicCredentialType::kLocalPublicCredential;
  CredentialStorageImpl creds_storage;
  creds_storage.GetPrivateCredentials(cred_selector,
                                      get_private_creds_callback);
  EXPECT_TRUE(get_private_cred_failed);
  creds_storage.SaveCredentials(cred_selector.manager_app_id,
                                cred_selector.account_name, empty_private_creds,
                                empty_public_creds, public_credential_type,
                                generate_creds_callback);
  EXPECT_FALSE(successfull_save);
  creds_storage.SaveCredentials(
      cred_selector.manager_app_id, cred_selector.account_name,
      default_private_creds, empty_public_creds, public_credential_type,
      generate_creds_callback);
  EXPECT_FALSE(successfull_save);
  creds_storage.SaveCredentials(
      cred_selector.manager_app_id, cred_selector.account_name,
      default_private_creds, default_public_creds, public_credential_type,
      std::move(generate_creds_callback));
  EXPECT_TRUE(successfull_save);
  creds_storage.GetPrivateCredentials(cred_selector,
                                      std::move(get_private_creds_callback));
  EXPECT_EQ(retrieved_creds.size(), 1);
}

TEST(CredentialStorageImplTest, CanSaveAndGetPublicCredentials) {
  std::vector<PrivateCredential> default_private_creds =
      BuildDefaultPrivateCreds();
  std::vector<PublicCredential> default_public_creds =
      BuildDefaultPublicCreds();
  std::vector<PrivateCredential> empty_private_creds;
  std::vector<PublicCredential> empty_public_creds;
  bool successfull_save = false;
  GenerateCredentialsCallback generate_creds_callback;
  generate_creds_callback.credentials_generated_cb =
      [&successfull_save, &default_public_creds](
          const std::vector<PublicCredential>& public_creds) {
        if (public_creds[0].secret_id() ==
            default_public_creds[0].secret_id()) {
          successfull_save = true;
        }
      };
  std::vector<PublicCredential> retrieved_creds;
  GetPublicCredentialsResultCallback get_public_creds_callback;
  get_public_creds_callback.credentials_fetched_cb =
      [&retrieved_creds](const std::vector<PublicCredential>& public_creds) {
        retrieved_creds = public_creds;
      };
  bool get_public_cred_failed = false;
  get_public_creds_callback.get_credentials_failed_cb =
      [&get_public_cred_failed](CredentialOperationStatus status) {
        if (status == CredentialOperationStatus::kFailed) {
          get_public_cred_failed = true;
        }
      };
  // Create CredentialStorageImpl object to test SaveCredentials &
  // GetPublicCredentials
  CredentialSelector cred_selector = BuildDefaultCredentialSelector();
  PublicCredentialType public_credential_type =
      PublicCredentialType::kLocalPublicCredential;
  CredentialStorageImpl creds_storage;
  creds_storage.GetPublicCredentials(cred_selector, public_credential_type,
                                     get_public_creds_callback);
  EXPECT_TRUE(get_public_cred_failed);
  creds_storage.SaveCredentials(cred_selector.manager_app_id,
                                cred_selector.account_name, empty_private_creds,
                                empty_public_creds, public_credential_type,
                                generate_creds_callback);
  EXPECT_FALSE(successfull_save);
  creds_storage.SaveCredentials(
      cred_selector.manager_app_id, cred_selector.account_name,
      default_private_creds, empty_public_creds, public_credential_type,
      generate_creds_callback);
  EXPECT_FALSE(successfull_save);
  creds_storage.SaveCredentials(
      cred_selector.manager_app_id, cred_selector.account_name,
      default_private_creds, default_public_creds, public_credential_type,
      std::move(generate_creds_callback));
  EXPECT_TRUE(successfull_save);
  creds_storage.GetPublicCredentials(cred_selector, public_credential_type,
                                     std::move(get_public_creds_callback));
  EXPECT_EQ(retrieved_creds.size(), 1);
}

TEST(CredentialStorageImplTest, OverwritePrivateCredentials) {
  std::vector<PrivateCredential> default_priv_creds =
      BuildDefaultPrivateCreds();
  std::vector<PublicCredential> default_pub_creds = BuildDefaultPublicCreds();
  PrivateCredential overwrite_priv_cred;
  overwrite_priv_cred.set_secret_id("overwrite_secret_id");
  std::vector<PrivateCredential> overwrite_priv_creds;
  overwrite_priv_creds.push_back(overwrite_priv_cred);
  std::vector<PublicCredential> generated_creds;
  GenerateCredentialsCallback generate_creds_callback;
  generate_creds_callback.credentials_generated_cb =
      [&generated_creds](const std::vector<PublicCredential>& public_creds) {
        generated_creds = public_creds;
      };
  std::vector<PrivateCredential> retrieved_creds;
  GetPrivateCredentialsResultCallback get_private_creds_callback;
  get_private_creds_callback.credentials_fetched_cb =
      [&retrieved_creds](const std::vector<PrivateCredential>& private_creds) {
        retrieved_creds = private_creds;
      };
  bool get_private_cred_failed = false;
  get_private_creds_callback.get_credentials_failed_cb =
      [&get_private_cred_failed](CredentialOperationStatus status) {
        if (status == CredentialOperationStatus::kFailed) {
          get_private_cred_failed = true;
        }
      };
  // Create CredentialStorageImpl object to test SaveCredentials &
  // GetPrivateCredentials
  CredentialSelector cred_selector = BuildDefaultCredentialSelector();
  PublicCredentialType public_credential_type =
      PublicCredentialType::kLocalPublicCredential;
  CredentialStorageImpl creds_storage;
  creds_storage.SaveCredentials(cred_selector.manager_app_id,
                                cred_selector.account_name, default_priv_creds,
                                default_pub_creds, public_credential_type,
                                generate_creds_callback);
  EXPECT_EQ(generated_creds[0].secret_id(), default_pub_creds[0].secret_id());
  creds_storage.GetPrivateCredentials(cred_selector,
                                      get_private_creds_callback);
  EXPECT_EQ(retrieved_creds[0].secret_id(), default_priv_creds[0].secret_id());
  creds_storage.SaveCredentials(
      cred_selector.manager_app_id, cred_selector.account_name,
      overwrite_priv_creds, default_pub_creds, public_credential_type,
      generate_creds_callback);
  EXPECT_EQ(generated_creds[0].secret_id(), default_pub_creds[0].secret_id());
  creds_storage.GetPrivateCredentials(cred_selector,
                                      get_private_creds_callback);
  EXPECT_EQ(retrieved_creds[0].secret_id(),
            overwrite_priv_creds[0].secret_id());
}

TEST(CredentialStorageImplTest, OverwritePublicCredentials) {
  std::vector<PrivateCredential> default_priv_creds =
      BuildDefaultPrivateCreds();
  std::vector<PublicCredential> default_pub_creds = BuildDefaultPublicCreds();
  PublicCredential overwrite_pub_cred;
  overwrite_pub_cred.set_secret_id("overwrite_secret_id");
  std::vector<PublicCredential> overwrite_pub_creds;
  overwrite_pub_creds.push_back(overwrite_pub_cred);
  std::vector<PublicCredential> generated_creds;
  GenerateCredentialsCallback generate_creds_callback;
  generate_creds_callback.credentials_generated_cb =
      [&generated_creds](const std::vector<PublicCredential>& public_creds) {
        generated_creds = public_creds;
      };
  std::vector<PublicCredential> retrieved_creds;
  GetPublicCredentialsResultCallback get_public_creds_callback;
  get_public_creds_callback.credentials_fetched_cb =
      [&retrieved_creds](const std::vector<PublicCredential>& public_creds) {
        retrieved_creds = public_creds;
      };
  bool get_public_cred_failed = false;
  get_public_creds_callback.get_credentials_failed_cb =
      [&get_public_cred_failed](CredentialOperationStatus status) {
        if (status == CredentialOperationStatus::kFailed) {
          get_public_cred_failed = true;
        }
      };
  // Create CredentialStorageImpl object to test SaveCredentials &
  // GetPublicCredentials
  CredentialSelector cred_selector = BuildDefaultCredentialSelector();
  PublicCredentialType public_credential_type =
      PublicCredentialType::kLocalPublicCredential;
  CredentialStorageImpl creds_storage;
  creds_storage.SaveCredentials(cred_selector.manager_app_id,
                                cred_selector.account_name, default_priv_creds,
                                default_pub_creds, public_credential_type,
                                generate_creds_callback);
  EXPECT_EQ(generated_creds[0].secret_id(), default_pub_creds[0].secret_id());
  creds_storage.GetPublicCredentials(cred_selector, public_credential_type,
                                     get_public_creds_callback);
  EXPECT_EQ(retrieved_creds[0].secret_id(), default_pub_creds[0].secret_id());
  creds_storage.SaveCredentials(cred_selector.manager_app_id,
                                cred_selector.account_name, default_priv_creds,
                                overwrite_pub_creds, public_credential_type,
                                generate_creds_callback);
  EXPECT_EQ(generated_creds[0].secret_id(), overwrite_pub_creds[0].secret_id());
  creds_storage.GetPublicCredentials(cred_selector, public_credential_type,
                                     get_public_creds_callback);
  EXPECT_EQ(retrieved_creds[0].secret_id(), overwrite_pub_creds[0].secret_id());
}

}  // namespace
}  // namespace nearby
}  // namespace location
