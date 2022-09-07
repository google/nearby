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

#include "presence/implementation/credential_manager_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "net/proto2/contrib/parse_proto/testing.h"
#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/credential_storage_impl.h"
#include "internal/platform/implementation/credential_storage.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/proto/credential.pb.h"

namespace nearby {
namespace presence {
namespace {
using ::location::nearby::Crypto;
using ::location::nearby::api::PublicCredentialType;
using ::location::nearby::api::SaveCredentialsResultCallback;
using ::nearby::internal::DeviceMetadata;
using ::nearby::internal::PrivateCredential;
using ::nearby::internal::PublicCredential;
using ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE;
using ::proto2::contrib::parse_proto::ParseTestProto;
using ::protobuf_matchers::EqualsProto;

DeviceMetadata CreateTestDeviceMetadata() {
  DeviceMetadata device_metadata;
  device_metadata.set_stable_device_id("test_device_id");
  device_metadata.set_account_name("test_account");
  device_metadata.set_device_name("NP test device");
  device_metadata.set_icon_url("test_image.test.com");
  device_metadata.set_bluetooth_mac_address("FF:FF:FF:FF:FF:FF");
  device_metadata.set_device_type(internal::DeviceMetadata::PHONE);
  return device_metadata;
}

class CredentialManagerImplTest : public ::testing::Test {
 public:
  class MockCredentialStorage : public location::nearby::CredentialStorageImpl {
   public:
    MOCK_METHOD(void, SaveCredentials,
                (absl::string_view manager_app_id,
                 absl::string_view account_name,
                 const std::vector<::nearby::internal::PrivateCredential>&
                     private_credentials,
                 const std::vector<::nearby::internal::PublicCredential>&
                     public_credentials,
                 PublicCredentialType public_credential_type,
                 SaveCredentialsResultCallback callback),
                (override));
  };

  CredentialManagerImplTest() {
    mock_credential_storage_ptr_ = std::make_unique<MockCredentialStorage>();
  }

 protected:
  std::unique_ptr<MockCredentialStorage> mock_credential_storage_ptr_;
};

// TODO(b/241926454): Make sure CredentialManager builds with Github.
TEST(CredentialManagerImpl, CreateOneCredentialSuccessfully) {
  DeviceMetadata device_metadata = CreateTestDeviceMetadata();

  CredentialManagerImpl credential_manager;
  auto credentials = credential_manager.CreatePrivateCredential(
      device_metadata, IDENTITY_TYPE_PRIVATE, /* start_time_ms= */ 0,
      /* end_time_ms= */ 1000);

  PrivateCredential* private_credential = credentials.first.get();

  // Verify the private credential.
  EXPECT_THAT(private_credential->device_metadata(),
              EqualsProto(device_metadata));
  EXPECT_EQ(private_credential->identity_type(), IDENTITY_TYPE_PRIVATE);
  EXPECT_FALSE(private_credential->secret_id().empty());
  EXPECT_EQ(private_credential->start_time_millis(), 0);
  EXPECT_EQ(private_credential->end_time_millis(), 1000);
  EXPECT_EQ(private_credential->authenticity_key().size(),
            CredentialManagerImpl::kAuthenticityKeyByteSize);
  EXPECT_FALSE(private_credential->verification_key().empty());
  EXPECT_EQ(private_credential->metadata_encryption_key().size(),
            CredentialManagerImpl::kAuthenticityKeyByteSize);

  PublicCredential* public_credential = credentials.second.get();
  // Verify the public credential.
  EXPECT_EQ(public_credential->identity_type(), IDENTITY_TYPE_PRIVATE);
  EXPECT_FALSE(public_credential->secret_id().empty());
  EXPECT_EQ(private_credential->authenticity_key(),
            public_credential->authenticity_key());
  EXPECT_EQ(public_credential->start_time_millis(), 0);
  EXPECT_EQ(public_credential->end_time_millis(), 1000);
  EXPECT_EQ(Crypto::Sha256(private_credential->metadata_encryption_key())
                .AsStringView(),
            public_credential->metadata_encryption_key_tag());
  EXPECT_FALSE(public_credential->verification_key().empty());
  EXPECT_FALSE(public_credential->encrypted_metadata_bytes().empty());

  // Decrypt the device metadata

  auto decrypted_device_metadata = credential_manager.DecryptDeviceMetadata(
      private_credential->metadata_encryption_key(),
      public_credential->authenticity_key(),
      public_credential->encrypted_metadata_bytes());

  EXPECT_EQ(private_credential->device_metadata().SerializeAsString(),
            decrypted_device_metadata);
}

TEST(CredentialManagerImpl, GenerateCredentialsSuccessfully) {
  DeviceMetadata device_metadata = CreateTestDeviceMetadata();

  CredentialManagerImpl credential_manager;

  GenerateCredentialsCallback credentials_generated_cb;

  std::vector<nearby::internal::PublicCredential> publicCredentials;

  auto create_creds_callback_lambda =
      [&publicCredentials](
          std::vector<nearby::internal::PublicCredential> credentials) {
        publicCredentials = credentials;
      };

  credentials_generated_cb.credentials_generated_cb =
      create_creds_callback_lambda;
  std::vector<internal::IdentityType> identityTypes{IDENTITY_TYPE_PRIVATE};

  credential_manager.GenerateCredentials(
      device_metadata,
      /* manager_app_id= */ "TEST_MANAGER_APP", identityTypes, 1, 2,
      credentials_generated_cb);

  EXPECT_EQ(publicCredentials.size(), 2);
  for (auto& public_credential : publicCredentials) {
    EXPECT_EQ(public_credential.identity_type(), IDENTITY_TYPE_PRIVATE);
    EXPECT_FALSE(public_credential.secret_id().empty());
    EXPECT_EQ(public_credential.end_time_millis() -
                  public_credential.start_time_millis(),
              1 * 24 * 3600 * 1000);
    EXPECT_FALSE(public_credential.encrypted_metadata_bytes().empty());
  }
}

TEST(CredentialManagerImpl, GenerateCredentialsSuccessfullyButStoreFailed) {
  DeviceMetadata device_metadata = CreateTestDeviceMetadata();

  auto credential_storage_ptr =
      std::make_unique<CredentialManagerImplTest::MockCredentialStorage>();

  EXPECT_CALL(*credential_storage_ptr, SaveCredentials)
      .WillOnce(::testing::Invoke(
          [](absl::string_view manager_app_id, absl::string_view account_name,
             const std::vector<PrivateCredential>& private_credentials,
             const std::vector<PublicCredential>& public_credentials,
             PublicCredentialType public_credential_type,
             SaveCredentialsResultCallback callback) {
            callback.credentials_saved_cb(
                location::nearby::api::CredentialOperationStatus::kFailed);
          }));
  CredentialManagerImpl credential_manager(std::move(credential_storage_ptr));

  GenerateCredentialsCallback credentials_generated_cb;
  std::vector<nearby::internal::PublicCredential> publicCredentials;
  auto create_creds_callback_lambda =
      [&publicCredentials](
          std::vector<nearby::internal::PublicCredential> credentials) {
        publicCredentials = credentials;
      };

  credentials_generated_cb.credentials_generated_cb =
      create_creds_callback_lambda;
  std::vector<internal::IdentityType> identityTypes{IDENTITY_TYPE_PRIVATE};

  credential_manager.GenerateCredentials(
      device_metadata,
      /* manager_app_id= */ "TEST_MANAGER_APP", identityTypes, 1, 2,
      credentials_generated_cb);
  EXPECT_TRUE(publicCredentials.empty());
}
}  // namespace

}  // namespace presence
}  // namespace nearby
