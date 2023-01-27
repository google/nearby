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

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/proto/credential.pb.h"

namespace nearby {
namespace {

using ::nearby::internal::IdentityType;
using ::nearby::internal::LocalCredential;
using ::nearby::internal::SharedCredential;
using ::nearby::presence::CredentialSelector;
using ::nearby::presence::GetLocalCredentialsResultCallback;
using ::nearby::presence::GetPublicCredentialsResultCallback;
using ::nearby::presence::PublicCredentialType;
using ::nearby::presence::SaveCredentialsResultCallback;
using ::protobuf_matchers::EqualsProto;
using ::testing::UnorderedPointwise;
using ::testing::status::StatusIs;

constexpr absl::string_view kSecretId = "secret id";
constexpr absl::string_view kManagerAppId = "manager app id";
constexpr absl::string_view kAccountName = "test_account";

// `secret_id` is used to create credentials with different content.
LocalCredential CreateLocalCredential(absl::string_view secret_id,
                                      IdentityType identity_type) {
  LocalCredential private_credential;
  private_credential.set_secret_id(secret_id);
  private_credential.set_identity_type(identity_type);
  return private_credential;
}

SharedCredential CreatePublicCredential(absl::string_view secret_id,
                                        IdentityType identity_type) {
  SharedCredential public_credential;
  public_credential.set_secret_id(secret_id);
  public_credential.set_identity_type(identity_type);
  return public_credential;
}

std::vector<LocalCredential> BuildPrivateCreds(absl::string_view secret_id) {
  std::vector<LocalCredential> private_credentials = {
      CreateLocalCredential(secret_id, IdentityType::IDENTITY_TYPE_PRIVATE),
      CreateLocalCredential(secret_id, IdentityType::IDENTITY_TYPE_TRUSTED),
      CreateLocalCredential(secret_id,
                            IdentityType::IDENTITY_TYPE_PROVISIONED)};
  return private_credentials;
}

std::vector<SharedCredential> BuildPublicCreds(absl::string_view secret_id) {
  std::vector<SharedCredential> public_credentials = {
      CreatePublicCredential(secret_id, IdentityType::IDENTITY_TYPE_PRIVATE),
      CreatePublicCredential(secret_id, IdentityType::IDENTITY_TYPE_TRUSTED),
      CreatePublicCredential(secret_id,
                             IdentityType::IDENTITY_TYPE_PROVISIONED)};
  return public_credentials;
}

absl::StatusOr<std::vector<LocalCredential>> GetLocalCredentials(
    CredentialStorageImpl& credential_storage, IdentityType identity_type,
    absl::string_view manager_app_id = kManagerAppId,
    absl::string_view account_name = kAccountName) {
  CredentialSelector selector = {.manager_app_id = std::string(manager_app_id),
                                 .account_name = std::string(account_name),
                                 .identity_type = identity_type};
  absl::StatusOr<std::vector<LocalCredential>> private_credentials;
  credential_storage.GetLocalCredentials(
      selector,
      GetLocalCredentialsResultCallback{
          .credentials_fetched_cb =
              [&](absl::StatusOr<std::vector<LocalCredential>> credentials) {
                private_credentials = std::move(credentials);
              }});
  return private_credentials;
}

absl::StatusOr<std::vector<SharedCredential>> GetPublicCredentials(
    CredentialStorageImpl& credential_storage, IdentityType identity_type,
    PublicCredentialType credential_type,
    absl::string_view manager_app_id = kManagerAppId,
    absl::string_view account_name = kAccountName) {
  CredentialSelector selector = {.manager_app_id = std::string(manager_app_id),
                                 .account_name = std::string(account_name),
                                 .identity_type = identity_type};
  absl::StatusOr<std::vector<SharedCredential>> public_credentials;
  credential_storage.GetPublicCredentials(
      selector, credential_type,
      GetPublicCredentialsResultCallback{
          .credentials_fetched_cb =
              [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
                public_credentials = std::move(credentials);
              }});
  return public_credentials;
}

absl::Status SaveCredentials(CredentialStorageImpl& credential_storage,
                             absl::string_view manager_app_id,
                             absl::string_view account_name,
                             std::vector<LocalCredential> private_credentials,
                             std::vector<SharedCredential> public_credentials,
                             PublicCredentialType public_credential_type) {
  absl::Status save_status = absl::UnknownError("");

  credential_storage.SaveCredentials(
      manager_app_id, account_name, private_credentials, public_credentials,
      public_credential_type,
      SaveCredentialsResultCallback{
          .credentials_saved_cb = [&](absl::Status status) {
            save_status = status;
          }});
  return save_status;
}

absl::Status SaveCredentials(CredentialStorageImpl& credential_storage,
                             std::vector<LocalCredential> private_credentials,
                             std::vector<SharedCredential> public_credentials) {
  return SaveCredentials(credential_storage, kManagerAppId, kAccountName,
                         private_credentials, public_credentials,
                         PublicCredentialType::kLocalPublicCredential);
}

absl::Status SaveLocalCredentials(CredentialStorageImpl& credential_storage,
                                  absl::string_view secret_id) {
  return SaveCredentials(credential_storage, kManagerAppId, kAccountName,
                         BuildPrivateCreds(secret_id),
                         std::vector<SharedCredential>(),
                         PublicCredentialType::kLocalPublicCredential);
}

absl::Status SavePublicCredentials(CredentialStorageImpl& credential_storage,
                                   PublicCredentialType credential_type,
                                   absl::string_view secret_id) {
  return SaveCredentials(credential_storage, kManagerAppId, kAccountName,
                         std::vector<LocalCredential>(),
                         BuildPublicCreds(secret_id), credential_type);
}

TEST(CredentialStorageImplTest, SaveAndGetLocalCredentials) {
  std::vector<LocalCredential> default_private_creds =
      BuildPrivateCreds(kSecretId);
  std::vector<SharedCredential> empty_public_creds;
  CredentialStorageImpl credential_storage;
  absl::Status save_status = absl::UnknownError("");

  credential_storage.SaveCredentials(
      kManagerAppId, kAccountName, default_private_creds, empty_public_creds,
      PublicCredentialType::kLocalPublicCredential,
      SaveCredentialsResultCallback{
          .credentials_saved_cb = [&](absl::Status status) {
            save_status = status;
          }});

  EXPECT_OK(save_status);
  auto fetched_private_credentials = GetLocalCredentials(
      credential_storage, IdentityType::IDENTITY_TYPE_UNSPECIFIED);
  ASSERT_OK(fetched_private_credentials);
  EXPECT_THAT(*fetched_private_credentials,
              UnorderedPointwise(EqualsProto(), default_private_creds));
}

TEST(CredentialStorageImplTest, UpdateLocalCredential) {
  std::vector<LocalCredential> default_private_creds =
      BuildPrivateCreds(kSecretId);
  std::vector<SharedCredential> empty_public_creds;
  CredentialStorageImpl credential_storage;
  absl::Status update_status = absl::UnknownError("");
  credential_storage.SaveCredentials(
      kManagerAppId, kAccountName, default_private_creds, empty_public_creds,
      PublicCredentialType::kLocalPublicCredential,
      SaveCredentialsResultCallback{
          .credentials_saved_cb = [&](absl::Status status) {
            ASSERT_OK(status);
          }});

  // Modify a private credential
  default_private_creds[0].mutable_consumed_salts()->insert({1234, true});
  credential_storage.UpdateLocalCredential(
      kManagerAppId, kAccountName, default_private_creds[0],
      {[&](absl::Status status) { update_status = status; }});

  EXPECT_OK(update_status);
  auto fetched_private_credentials = GetLocalCredentials(
      credential_storage, IdentityType::IDENTITY_TYPE_UNSPECIFIED);
  ASSERT_OK(fetched_private_credentials);
  EXPECT_THAT(*fetched_private_credentials,
              UnorderedPointwise(EqualsProto(), default_private_creds));
}

TEST(CredentialStorageImplTest, ReplaceAndGetLocalCredentials) {
  constexpr absl::string_view kAnotherSecretId = "another secret id";
  CredentialStorageImpl credential_storage;

  EXPECT_OK(SaveLocalCredentials(credential_storage, kSecretId));
  EXPECT_OK(SaveLocalCredentials(credential_storage, kAnotherSecretId));

  auto fetched_private_credentials = GetLocalCredentials(
      credential_storage, IdentityType::IDENTITY_TYPE_UNSPECIFIED);
  ASSERT_OK(fetched_private_credentials);
  EXPECT_THAT(
      *fetched_private_credentials,
      UnorderedPointwise(EqualsProto(), BuildPrivateCreds(kAnotherSecretId)));
}

TEST(CredentialStorageImplTest, SaveAndGetLocalPublicCredentials) {
  std::vector<LocalCredential> empty_private_creds;
  std::vector<SharedCredential> public_creds = BuildPublicCreds(kSecretId);
  CredentialStorageImpl credential_storage;
  absl::Status save_status = absl::UnknownError("");

  credential_storage.SaveCredentials(
      kManagerAppId, kAccountName, empty_private_creds, public_creds,
      PublicCredentialType::kLocalPublicCredential,
      SaveCredentialsResultCallback{
          .credentials_saved_cb = [&](absl::Status status) {
            save_status = status;
          }});

  EXPECT_OK(save_status);
  auto fetched_public_credentials = GetPublicCredentials(
      credential_storage, IdentityType::IDENTITY_TYPE_UNSPECIFIED,
      PublicCredentialType::kLocalPublicCredential);
  ASSERT_OK(fetched_public_credentials);
  EXPECT_THAT(*fetched_public_credentials,
              UnorderedPointwise(EqualsProto(), public_creds));
}

TEST(CredentialStorageImplTest, ReplaceAndGetLocalPublicCredentials) {
  constexpr absl::string_view kAnotherSecretId = "another secret id";
  CredentialStorageImpl credential_storage;

  EXPECT_OK(SavePublicCredentials(credential_storage,
                                  PublicCredentialType::kLocalPublicCredential,
                                  kSecretId));
  EXPECT_OK(SavePublicCredentials(credential_storage,
                                  PublicCredentialType::kLocalPublicCredential,
                                  kAnotherSecretId));

  auto fetched_public_credentials = GetPublicCredentials(
      credential_storage, IdentityType::IDENTITY_TYPE_UNSPECIFIED,
      PublicCredentialType::kLocalPublicCredential);
  ASSERT_OK(fetched_public_credentials);
  EXPECT_THAT(
      *fetched_public_credentials,
      UnorderedPointwise(EqualsProto(), BuildPublicCreds(kAnotherSecretId)));
}

TEST(CredentialStorageImplTest, SaveAndGetRemotePublicCredentials) {
  std::vector<LocalCredential> empty_private_creds;
  std::vector<SharedCredential> public_creds = BuildPublicCreds(kSecretId);
  CredentialStorageImpl credential_storage;
  absl::Status save_status = absl::UnknownError("");

  credential_storage.SaveCredentials(
      kManagerAppId, kAccountName, empty_private_creds, public_creds,
      PublicCredentialType::kRemotePublicCredential,
      SaveCredentialsResultCallback{
          .credentials_saved_cb = [&](absl::Status status) {
            save_status = status;
          }});

  EXPECT_OK(save_status);
  auto fetched_public_credentials = GetPublicCredentials(
      credential_storage, IdentityType::IDENTITY_TYPE_UNSPECIFIED,
      PublicCredentialType::kRemotePublicCredential);
  ASSERT_OK(fetched_public_credentials);
  EXPECT_THAT(*fetched_public_credentials,
              UnorderedPointwise(EqualsProto(), public_creds));
}

TEST(CredentialStorageImplTest, ReplaceAndGetRemotePublicCredentials) {
  constexpr absl::string_view kAnotherSecretId = "another secret id";
  CredentialStorageImpl credential_storage;

  EXPECT_OK(SavePublicCredentials(credential_storage,
                                  PublicCredentialType::kRemotePublicCredential,
                                  kSecretId));
  EXPECT_OK(SavePublicCredentials(credential_storage,
                                  PublicCredentialType::kRemotePublicCredential,
                                  kAnotherSecretId));

  auto fetched_public_credentials = GetPublicCredentials(
      credential_storage, IdentityType::IDENTITY_TYPE_UNSPECIFIED,
      PublicCredentialType::kRemotePublicCredential);
  ASSERT_OK(fetched_public_credentials);
  EXPECT_THAT(
      *fetched_public_credentials,
      UnorderedPointwise(EqualsProto(), BuildPublicCreds(kAnotherSecretId)));
}

TEST(CredentialStorageImplTest, SavePrivateAndLocalPublicCredentials) {
  std::vector<LocalCredential> private_creds = BuildPrivateCreds(kSecretId);
  std::vector<SharedCredential> public_creds = BuildPublicCreds(kSecretId);
  CredentialStorageImpl credential_storage;

  absl::Status status =
      SaveCredentials(credential_storage, private_creds, public_creds);

  EXPECT_OK(status);
  auto fetched_public_credentials = GetPublicCredentials(
      credential_storage, IdentityType::IDENTITY_TYPE_UNSPECIFIED,
      PublicCredentialType::kLocalPublicCredential);
  ASSERT_OK(fetched_public_credentials);
  EXPECT_THAT(*fetched_public_credentials,
              UnorderedPointwise(EqualsProto(), public_creds));
  auto fetched_private_credentials = GetLocalCredentials(
      credential_storage, IdentityType::IDENTITY_TYPE_UNSPECIFIED);
  ASSERT_OK(fetched_private_credentials);
  EXPECT_THAT(*fetched_private_credentials,
              UnorderedPointwise(EqualsProto(), private_creds));
}

TEST(CredentialStorageImplTest, SaveCredentialsFailsWhenNoCredentials) {
  std::vector<LocalCredential> empty_private_creds;
  std::vector<SharedCredential> empty_public_creds;
  CredentialStorageImpl credential_storage;
  absl::Status save_status = absl::UnknownError("");

  credential_storage.SaveCredentials(
      kManagerAppId, kAccountName, empty_private_creds, empty_public_creds,
      PublicCredentialType::kLocalPublicCredential,
      SaveCredentialsResultCallback{
          .credentials_saved_cb = [&](absl::Status status) {
            save_status = status;
          }});

  EXPECT_THAT(save_status, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(CredentialStorageImplTest, GetLocalCredentialsFailsWhenNoCredentials) {
  CredentialStorageImpl credential_storage;
  EXPECT_OK(SavePublicCredentials(credential_storage,
                                  PublicCredentialType::kRemotePublicCredential,
                                  kSecretId));

  EXPECT_THAT(GetLocalCredentials(credential_storage,
                                  IdentityType::IDENTITY_TYPE_UNSPECIFIED),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST(CredentialStorageImplTest,
     GetLocalCredentialsFailsWhenManagerAppIdDoesNotMatch) {
  CredentialStorageImpl credential_storage;
  EXPECT_OK(SaveCredentials(credential_storage, BuildPrivateCreds(kSecretId),
                            BuildPublicCreds(kSecretId)));

  EXPECT_THAT(GetLocalCredentials(credential_storage,
                                  IdentityType::IDENTITY_TYPE_UNSPECIFIED,
                                  "different manager app id", kAccountName),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST(CredentialStorageImplTest,
     GetLocalCredentialsFailsWhenAccountNameDoesNotMatch) {
  CredentialStorageImpl credential_storage;
  EXPECT_OK(SaveCredentials(credential_storage, BuildPrivateCreds(kSecretId),
                            BuildPublicCreds(kSecretId)));

  EXPECT_THAT(GetLocalCredentials(credential_storage,
                                  IdentityType::IDENTITY_TYPE_UNSPECIFIED,
                                  kManagerAppId, "different account name"),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST(CredentialStorageImplTest,
     GetPublicCredentialsFailsWhenManagerAppIdDoesNotMatch) {
  CredentialStorageImpl credential_storage;
  EXPECT_OK(SaveCredentials(credential_storage, BuildPrivateCreds(kSecretId),
                            BuildPublicCreds(kSecretId)));

  EXPECT_THAT(GetPublicCredentials(credential_storage,
                                   IdentityType::IDENTITY_TYPE_UNSPECIFIED,
                                   PublicCredentialType::kLocalPublicCredential,
                                   "different manager app id", kAccountName),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST(CredentialStorageImplTest,
     GetPublicCredentialsFailsWhenAccountNameDoesNotMatch) {
  CredentialStorageImpl credential_storage;
  EXPECT_OK(SaveCredentials(credential_storage, BuildPrivateCreds(kSecretId),
                            BuildPublicCreds(kSecretId)));

  EXPECT_THAT(GetPublicCredentials(credential_storage,
                                   IdentityType::IDENTITY_TYPE_UNSPECIFIED,
                                   PublicCredentialType::kLocalPublicCredential,
                                   kManagerAppId, "different account name"),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST(CredentialStorageImplTest, GetPublicCredentialsFailsWhenNoCredentials) {
  CredentialStorageImpl credential_storage;
  EXPECT_OK(SavePublicCredentials(credential_storage,
                                  PublicCredentialType::kRemotePublicCredential,
                                  kSecretId));

  EXPECT_THAT(GetPublicCredentials(
                  credential_storage, IdentityType::IDENTITY_TYPE_UNSPECIFIED,
                  PublicCredentialType::kLocalPublicCredential),
              StatusIs(absl::StatusCode::kNotFound));
}

class IdentityFilterTest : public testing::TestWithParam<IdentityType> {};

TEST_P(IdentityFilterTest, FilterLocalCredentialsByIdentityType) {
  IdentityType identity_type = GetParam();
  CredentialStorageImpl credential_storage;
  EXPECT_OK(SaveLocalCredentials(credential_storage, kSecretId));
  EXPECT_OK(SavePublicCredentials(credential_storage,
                                  PublicCredentialType::kLocalPublicCredential,
                                  kSecretId));

  auto private_credentials =
      GetLocalCredentials(credential_storage, identity_type);

  ASSERT_OK(private_credentials);
  EXPECT_THAT(
      *private_credentials,
      UnorderedPointwise(EqualsProto(),
                         std::vector<LocalCredential>{
                             CreateLocalCredential(kSecretId, identity_type)}));
}

TEST_P(IdentityFilterTest, FilterLocalCredentialsFailsWhenNoCredentialsMatch) {
  IdentityType identity_type = GetParam();
  // Create a credential of a different identity type than the one we query.
  IdentityType other_type = identity_type == IdentityType::IDENTITY_TYPE_PRIVATE
                                ? IdentityType::IDENTITY_TYPE_TRUSTED
                                : IdentityType::IDENTITY_TYPE_PRIVATE;
  std::vector<LocalCredential> private_creds = {
      CreateLocalCredential(kSecretId, other_type)};
  CredentialStorageImpl credential_storage;
  EXPECT_OK(SaveCredentials(credential_storage, private_creds,
                            BuildPublicCreds(kSecretId)));

  EXPECT_THAT(GetLocalCredentials(credential_storage, identity_type),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_P(IdentityFilterTest, FilterPublicCredentialsByIdentityType) {
  IdentityType identity_type = GetParam();
  CredentialStorageImpl credential_storage;
  EXPECT_OK(SaveLocalCredentials(credential_storage, kSecretId));
  EXPECT_OK(SavePublicCredentials(credential_storage,
                                  PublicCredentialType::kLocalPublicCredential,
                                  kSecretId));

  auto public_credentials =
      GetPublicCredentials(credential_storage, identity_type,
                           PublicCredentialType::kLocalPublicCredential);

  ASSERT_OK(public_credentials);
  EXPECT_THAT(
      *public_credentials,
      UnorderedPointwise(EqualsProto(),
                         std::vector<SharedCredential>{CreatePublicCredential(
                             kSecretId, identity_type)}));
}

TEST_P(IdentityFilterTest, FilterPublicCredentialsFailsWhenNoCredentialsMatch) {
  IdentityType identity_type = GetParam();
  // Create a credential of a different identity type than the one we query.
  IdentityType other_type = identity_type == IdentityType::IDENTITY_TYPE_PRIVATE
                                ? IdentityType::IDENTITY_TYPE_TRUSTED
                                : IdentityType::IDENTITY_TYPE_PRIVATE;
  std::vector<SharedCredential> public_creds = {
      CreatePublicCredential(kSecretId, other_type)};
  CredentialStorageImpl credential_storage;
  EXPECT_OK(SaveCredentials(credential_storage, BuildPrivateCreds(kSecretId),
                            public_creds));

  EXPECT_THAT(
      GetPublicCredentials(credential_storage, identity_type,
                           PublicCredentialType::kLocalPublicCredential),
      StatusIs(absl::StatusCode::kNotFound));
}

INSTANTIATE_TEST_SUITE_P(
    CredentialStorageImplTest, IdentityFilterTest,
    testing::Values(IdentityType::IDENTITY_TYPE_PRIVATE,
                    IdentityType::IDENTITY_TYPE_TRUSTED,
                    IdentityType::IDENTITY_TYPE_PROVISIONED));

}  // namespace
}  // namespace nearby
