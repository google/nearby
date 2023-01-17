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
#include <string>
#include <utility>
#include <vector>

#include "net/proto2/contrib/parse_proto/testing.h"
#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/credential_storage_impl.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/credential.proto.h"

namespace nearby {
namespace presence {
namespace {
using ::nearby::CountDownLatch;
using ::nearby::Crypto;
using ::nearby::MediumEnvironment;
using ::nearby::internal::DeviceMetadata;
using ::nearby::internal::IdentityType;
using ::nearby::internal::PrivateCredential;
using ::nearby::internal::PublicCredential;
using ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE;
using ::proto2::contrib::parse_proto::ParseTestProto;
using ::protobuf_matchers::EqualsProto;
using ::testing::status::StatusIs;

constexpr absl::string_view kManagerAppId = "TEST_MANAGER_APP";
constexpr absl::string_view kAccountName = "test account";

DeviceMetadata CreateTestDeviceMetadata(
    absl::string_view account_name = kAccountName) {
  DeviceMetadata device_metadata;
  device_metadata.set_stable_device_id("test_device_id");
  device_metadata.set_account_name(account_name);
  device_metadata.set_device_name("NP test device");
  device_metadata.set_icon_url("test_image.test.com");
  device_metadata.set_bluetooth_mac_address("FF:FF:FF:FF:FF:FF");
  device_metadata.set_device_type(internal::DeviceMetadata::PHONE);
  return device_metadata;
}

CredentialSelector BuildDefaultCredentialSelector() {
  CredentialSelector credential_selector;
  credential_selector.manager_app_id = std::string(kManagerAppId);
  credential_selector.account_name = std::string(kAccountName);
  credential_selector.identity_type = IDENTITY_TYPE_PRIVATE;
  return credential_selector;
}

class CredentialManagerImplTest : public ::testing::Test {
 public:
  class MockCredentialStorage : public nearby::CredentialStorageImpl {
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
    MOCK_METHOD(
        void, GetPublicCredentials,
        (const ::nearby::presence::CredentialSelector& credential_selector,
         ::nearby::presence::PublicCredentialType public_credential_type,
         ::nearby::presence::GetPublicCredentialsResultCallback callback),
        (override));
  };

  class MockCredentialManager : public CredentialManagerImpl {
   public:
    explicit MockCredentialManager(SingleThreadExecutor* executor)
        : CredentialManagerImpl(executor) {}
    MOCK_METHOD(std::string, EncryptDeviceMetadata,
                (absl::string_view device_metadata_encryption_key,
                 absl::string_view authenticity_key,
                 absl::string_view device_metadata_string),
                (override));
  };

  ~CredentialManagerImplTest() override { executor_.Shutdown(); }

  // Waits for active tasks in the background thread to complete.
  void Fence() {
    // A runnable on medium environment thread can add a task on "our" executor,
    // and vice-versa. We need to wait for tasks on both threads in a loop a few
    // times to make sure that all tasks have finished.
    for (int i = 0; i < 3; i++) {
      MediumEnvironment::Instance().Sync();
      CountDownLatch latch(1);
      executor_.Execute([&]() { latch.CountDown(); });
      latch.Await();
    }
  }

  void AddLocalIdentity(absl::string_view manager_app_id,
                        absl::string_view account_name,
                        IdentityType identity_type) {
    DeviceMetadata device_metadata = CreateTestDeviceMetadata(account_name);

    credential_manager_.GenerateCredentials(
        device_metadata, manager_app_id, {identity_type},
        /*credential_life_cycle_days=*/1,
        /*contigous_copy_of_credentials=*/1,
        {[](absl::StatusOr<std::vector<PublicCredential>> credentials) {
          EXPECT_OK(credentials);
        }});
  }

 protected:
  SingleThreadExecutor executor_;
  CredentialManagerImpl credential_manager_{&executor_};
  MockCredentialManager mock_credential_manager_{&executor_};
};

TEST_F(CredentialManagerImplTest, CreateOneCredentialSuccessfully) {
  DeviceMetadata device_metadata = CreateTestDeviceMetadata();

  auto credentials = credential_manager_.CreatePrivateCredential(
      device_metadata, IDENTITY_TYPE_PRIVATE, /* start_time_ms= */ 0,
      /* end_time_ms= */ 1000);

  PrivateCredential private_credential = credentials.first;

  // Verify the private credential.
  EXPECT_THAT(private_credential.device_metadata(),
              EqualsProto(device_metadata));
  EXPECT_EQ(private_credential.identity_type(), IDENTITY_TYPE_PRIVATE);
  EXPECT_FALSE(private_credential.secret_id().empty());
  EXPECT_EQ(private_credential.start_time_millis(), 0);
  EXPECT_EQ(private_credential.end_time_millis(), 1000);
  EXPECT_EQ(private_credential.authenticity_key().size(),
            CredentialManagerImpl::kAuthenticityKeyByteSize);
  EXPECT_FALSE(private_credential.verification_key().empty());
  EXPECT_EQ(private_credential.metadata_encryption_key().size(),
            CredentialManagerImpl::kAuthenticityKeyByteSize);

  PublicCredential public_credential = credentials.second;
  // Verify the public credential.
  EXPECT_EQ(public_credential.identity_type(), IDENTITY_TYPE_PRIVATE);
  EXPECT_FALSE(public_credential.secret_id().empty());
  EXPECT_EQ(private_credential.authenticity_key(),
            public_credential.authenticity_key());
  EXPECT_EQ(public_credential.start_time_millis(), 0);
  EXPECT_EQ(public_credential.end_time_millis(), 1000);
  EXPECT_EQ(Crypto::Sha256(private_credential.metadata_encryption_key())
                .AsStringView(),
            public_credential.metadata_encryption_key_tag());
  EXPECT_FALSE(public_credential.verification_key().empty());
  EXPECT_FALSE(public_credential.encrypted_metadata_bytes().empty());

  // Decrypt the device metadata

  auto decrypted_device_metadata = credential_manager_.DecryptDeviceMetadata(
      private_credential.metadata_encryption_key(),
      public_credential.authenticity_key(),
      public_credential.encrypted_metadata_bytes());

  EXPECT_EQ(private_credential.device_metadata().SerializeAsString(),
            decrypted_device_metadata);
}

TEST_F(CredentialManagerImplTest, GenerateCredentialsSuccessfully) {
  DeviceMetadata device_metadata = CreateTestDeviceMetadata();
  absl::StatusOr<std::vector<nearby::internal::PublicCredential>>
      public_credentials;
  std::vector<IdentityType> identityTypes{IDENTITY_TYPE_PRIVATE};

  credential_manager_.GenerateCredentials(
      device_metadata, kManagerAppId, identityTypes, 1, 2,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<nearby::internal::PublicCredential>>
                   credentials) {
             public_credentials = std::move(credentials);
           }});

  EXPECT_OK(public_credentials);
  EXPECT_EQ(public_credentials->size(), 2);
  for (auto& public_credential : *public_credentials) {
    EXPECT_EQ(public_credential.identity_type(), IDENTITY_TYPE_PRIVATE);
    EXPECT_FALSE(public_credential.secret_id().empty());
    EXPECT_EQ(public_credential.end_time_millis() -
                  public_credential.start_time_millis(),
              1 * 24 * 3600 * 1000);
    EXPECT_FALSE(public_credential.encrypted_metadata_bytes().empty());
  }
}

TEST_F(CredentialManagerImplTest,
       SubscribeCallsCallbackWithExistingCredentials) {
  absl::StatusOr<std::vector<PublicCredential>> public_credentials1;
  absl::StatusOr<std::vector<PublicCredential>> public_credentials2;
  AddLocalIdentity(kManagerAppId, kAccountName, IDENTITY_TYPE_PRIVATE);

  SubscriberId id1 = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{.manager_app_id = std::string(kManagerAppId),
                         .account_name = std::string(kAccountName),
                         .identity_type = IDENTITY_TYPE_PRIVATE},
      PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<PublicCredential>> credentials) {
             public_credentials1 = std::move(credentials);
           }});
  SubscriberId id2 = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{.manager_app_id = std::string(kManagerAppId),
                         .account_name = std::string(kAccountName),
                         .identity_type = IDENTITY_TYPE_PRIVATE},
      PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<PublicCredential>> credentials) {
             public_credentials2 = std::move(credentials);
           }});

  Fence();
  EXPECT_OK(public_credentials1);
  EXPECT_OK(public_credentials2);
  EXPECT_EQ(public_credentials1->size(), 1);
  EXPECT_EQ(public_credentials2->size(), 1);
  // Cleanup
  credential_manager_.UnsubscribeFromPublicCredentials(id1);
  credential_manager_.UnsubscribeFromPublicCredentials(id2);
  Fence();
}

TEST_F(CredentialManagerImplTest,
       SubscribeCallsCallbackWithUpdatedCredentials) {
  absl::StatusOr<std::vector<PublicCredential>> public_credentials;

  SubscriberId id = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{.manager_app_id = std::string(kManagerAppId),
                         .account_name = std::string(kAccountName),
                         .identity_type = IDENTITY_TYPE_PRIVATE},
      PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<PublicCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});
  Fence();
  EXPECT_THAT(public_credentials, StatusIs(absl::StatusCode::kUnknown));

  AddLocalIdentity(kManagerAppId, kAccountName, IDENTITY_TYPE_PRIVATE);

  Fence();
  ASSERT_OK(public_credentials);
  EXPECT_EQ(public_credentials->size(), 1);
  // Cleanup
  credential_manager_.UnsubscribeFromPublicCredentials(id);
  Fence();
}

TEST_F(CredentialManagerImplTest, NoCallbacksAfterUnsubscribe) {
  absl::StatusOr<std::vector<PublicCredential>> public_credentials;
  SubscriberId id = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{.manager_app_id = std::string(kManagerAppId),
                         .account_name = std::string(kAccountName),
                         .identity_type = IDENTITY_TYPE_PRIVATE},
      PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<PublicCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});

  credential_manager_.UnsubscribeFromPublicCredentials(id);
  AddLocalIdentity(kManagerAppId, kAccountName, IDENTITY_TYPE_PRIVATE);

  Fence();
  EXPECT_THAT(public_credentials, StatusIs(absl::StatusCode::kUnknown));
}

TEST_F(CredentialManagerImplTest,
       GenerateCredentialsSuccessfullyButStoreFailed) {
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
                absl::FailedPreconditionError("Expected failure"));
          }));
  credential_manager_ =
      CredentialManagerImpl(&executor_, std::move(credential_storage_ptr));
  absl::StatusOr<std::vector<nearby::internal::PublicCredential>>
      public_credentials;
  std::vector<IdentityType> identityTypes{IDENTITY_TYPE_PRIVATE};

  credential_manager_.GenerateCredentials(
      device_metadata, kManagerAppId, identityTypes, 1, 2,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<nearby::internal::PublicCredential>>
                   credentials) {
             public_credentials = std::move(credentials);
           }});
  EXPECT_THAT(public_credentials,
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(CredentialManagerImplTest, UpdateRemotePublicCredentialsSuccessfully) {
  nearby::internal::PublicCredential public_credential_for_test;
  public_credential_for_test.set_identity_type(
      nearby::internal::IdentityType::IDENTITY_TYPE_TRUSTED);
  std::vector<nearby::internal::PublicCredential> public_credentials{
      {public_credential_for_test}};

  nearby::CountDownLatch updated_latch(1);
  UpdateRemotePublicCredentialsCallback update_credentials_cb{
      .credentials_updated_cb =
          [&updated_latch](absl::Status status) {
            if (status.ok()) {
              updated_latch.CountDown();
            }
          },
  };

  credential_manager_.UpdateRemotePublicCredentials(
      kManagerAppId, kAccountName, public_credentials,
      std::move(update_credentials_cb));

  EXPECT_TRUE(updated_latch.Await().Ok());
}

TEST_F(CredentialManagerImplTest,
       UpdateRemotePublicCredentialsNotifiesSubscribers) {
  absl::StatusOr<std::vector<PublicCredential>> subscribed_credentials;
  nearby::internal::PublicCredential public_credential_for_test;
  public_credential_for_test.set_identity_type(
      nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE);
  std::vector<nearby::internal::PublicCredential> public_credentials{
      {public_credential_for_test}};
  nearby::CountDownLatch updated_latch(1);
  UpdateRemotePublicCredentialsCallback update_credentials_cb{
      .credentials_updated_cb =
          [&updated_latch](absl::Status status) {
            if (status.ok()) {
              updated_latch.CountDown();
            }
          },
  };
  SubscriberId id1 = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{.manager_app_id = std::string(kManagerAppId),
                         .account_name = std::string(kAccountName),
                         .identity_type = internal::IDENTITY_TYPE_PRIVATE},
      PublicCredentialType::kRemotePublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<PublicCredential>> credentials) {
             subscribed_credentials = std::move(credentials);
           }});
  SubscriberId id2 = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{.manager_app_id = std::string(kManagerAppId),
                         .account_name = std::string(kAccountName),
                         .identity_type = internal::IDENTITY_TYPE_TRUSTED},
      PublicCredentialType::kRemotePublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<PublicCredential>> credentials) {
             // This callback should not be called because there are no Trusted
             // credentials in this test.
             GTEST_FAIL();
           }});

  credential_manager_.UpdateRemotePublicCredentials(
      kManagerAppId, kAccountName, public_credentials,
      std::move(update_credentials_cb));

  EXPECT_TRUE(updated_latch.Await().Ok());
  Fence();
  EXPECT_OK(subscribed_credentials);
  EXPECT_EQ(subscribed_credentials->size(), 1);
  credential_manager_.UnsubscribeFromPublicCredentials(id1);
  credential_manager_.UnsubscribeFromPublicCredentials(id2);
}

TEST_F(CredentialManagerImplTest, GetPrivateCredentialsFailed) {
  absl::StatusOr<std::vector<PrivateCredential>> private_credentials;
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();

  credential_manager_.GetPrivateCredentials(
      credential_selector,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<PrivateCredential>> credentials) {
             private_credentials = std::move(credentials);
           }});

  EXPECT_THAT(private_credentials, StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CredentialManagerImplTest, GetPublicCredentialsFailed) {
  absl::StatusOr<std::vector<PublicCredential>> public_credentials;
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();

  credential_manager_.GetPublicCredentials(
      credential_selector, PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<PublicCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});

  EXPECT_THAT(public_credentials, StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CredentialManagerImplTest, GetCredentialsSuccessfully) {
  DeviceMetadata device_metadata = CreateTestDeviceMetadata();
  absl::StatusOr<std::vector<nearby::internal::PublicCredential>>
      public_credentials;
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE};
  absl::StatusOr<std::vector<PrivateCredential>> private_credentials;
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();

  credential_manager_.GenerateCredentials(
      device_metadata, kManagerAppId, identity_types, 1, 1,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<nearby::internal::PublicCredential>>
                   credentials) {
             public_credentials = std::move(credentials);
           }});
  credential_manager_.GetPrivateCredentials(
      credential_selector,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<PrivateCredential>> credentials) {
             private_credentials = std::move(credentials);
           }});

  EXPECT_OK(public_credentials);
  EXPECT_EQ(public_credentials->size(), 1);
  EXPECT_OK(private_credentials);
  EXPECT_FALSE(private_credentials->empty());
}

TEST_F(CredentialManagerImplTest, PublicCredentialsFailEncryption) {
  DeviceMetadata device_metadata = CreateTestDeviceMetadata();
  absl::StatusOr<std::vector<nearby::internal::PublicCredential>>
      public_credentials;
  auto credential_manager_ptr =
      std::make_unique<CredentialManagerImplTest::MockCredentialManager>(
          &executor_);
  EXPECT_CALL(*credential_manager_ptr, EncryptDeviceMetadata)
      .WillOnce(::testing::Invoke(
          [](absl::string_view device_metadata_encryption_key,
             absl::string_view authenticity_key,
             absl::string_view device_metadata_string) { return ""; }));
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE};

  credential_manager_ptr->GenerateCredentials(
      device_metadata, kManagerAppId, identity_types, 1, 1,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<nearby::internal::PublicCredential>>
                   credentials) {
             public_credentials = std::move(credentials);
           }});

  EXPECT_THAT(public_credentials, StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace

}  // namespace presence
}  // namespace nearby
