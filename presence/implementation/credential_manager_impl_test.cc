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

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "net/proto2/contrib/parse_proto/testing.h"
#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/credential_storage_impl.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/proto/credential.pb.h"
#include "presence/implementation/base_broadcast_request.h"

namespace nearby {
namespace presence {
namespace {
using ::nearby::CountDownLatch;
using ::nearby::Crypto;
using ::nearby::MediumEnvironment;
using ::nearby::internal::IdentityType;
using ::nearby::internal::LocalCredential;
using ::nearby::internal::Metadata;
using ::nearby::internal::SharedCredential;
using ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE;
using ::nearby::internal::IdentityType::IDENTITY_TYPE_TRUSTED;
using ::protobuf_matchers::EqualsProto;
using ::testing::UnorderedPointwise;
using ::testing::status::StatusIs;

constexpr absl::string_view kManagerAppId = "TEST_MANAGER_APP";
constexpr absl::string_view kAccountName = "test account";

Metadata CreateTestMetadata(absl::string_view account_name = kAccountName) {
  Metadata metadata;
  metadata.set_device_id("test_device_id");
  metadata.set_account_name(account_name);
  metadata.set_device_name("NP test device");
  metadata.set_device_profile_url("test_image.test.com");
  metadata.set_bluetooth_mac_address("FF:FF:FF:FF:FF:FF");
  return metadata;
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
                 const std::vector<LocalCredential>& private_credentials,
                 const std::vector<SharedCredential>& public_credentials,
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
    MOCK_METHOD(std::string, EncryptMetadata,
                (absl::string_view metadata_encryption_key,
                 absl::string_view key_seed, absl::string_view metadata_string),
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
    Metadata metadata = CreateTestMetadata(account_name);

    credential_manager_.GenerateCredentials(
        metadata, manager_app_id, {identity_type},
        /*credential_life_cycle_days=*/1,
        /*contigous_copy_of_credentials=*/1,
        {[](absl::StatusOr<std::vector<SharedCredential>> credentials) {
          EXPECT_OK(credentials);
        }});
  }

 protected:
  SingleThreadExecutor executor_;
  CredentialManagerImpl credential_manager_{&executor_};
  MockCredentialManager mock_credential_manager_{&executor_};
};

TEST_F(CredentialManagerImplTest, CreateOneCredentialSuccessfully) {
  Metadata metadata = CreateTestMetadata();
  constexpr absl::Time kStartTime = absl::FromUnixSeconds(100000);
  constexpr absl::Time kEndTime = absl::FromUnixSeconds(200000);

  auto credentials = credential_manager_.CreateLocalCredential(
      metadata, IDENTITY_TYPE_PRIVATE, kStartTime, kEndTime);

  LocalCredential private_credential = credentials.first;
  // Verify the private credential.
  EXPECT_EQ(private_credential.identity_type(), IDENTITY_TYPE_PRIVATE);
  EXPECT_FALSE(private_credential.secret_id().empty());
  EXPECT_EQ(private_credential.start_time_millis(),
            absl::ToUnixMillis(kStartTime));
  EXPECT_EQ(private_credential.end_time_millis(), absl::ToUnixMillis(kEndTime));
  EXPECT_EQ(private_credential.key_seed().size(),
            CredentialManagerImpl::kAuthenticityKeyByteSize);
  EXPECT_FALSE(private_credential.connection_signing_key().key().empty());
  EXPECT_EQ(private_credential.metadata_encryption_key().size(),
            kBaseMetadataSize);

  SharedCredential public_credential = credentials.second;
  // Verify the public credential.
  EXPECT_EQ(public_credential.identity_type(), IDENTITY_TYPE_PRIVATE);
  EXPECT_FALSE(public_credential.secret_id().empty());
  EXPECT_EQ(private_credential.key_seed(), public_credential.key_seed());
  EXPECT_LE(public_credential.start_time_millis(),
            absl::ToUnixMillis(kStartTime));
  EXPECT_GE(public_credential.start_time_millis(),
            absl::ToUnixMillis(kStartTime - absl::Hours(3)));
  EXPECT_GE(public_credential.end_time_millis(), absl::ToUnixMillis(kEndTime));
  EXPECT_LE(public_credential.end_time_millis(),
            absl::ToUnixMillis(kEndTime + absl::Hours(3)));
  EXPECT_EQ(Crypto::Sha256(private_credential.metadata_encryption_key())
                .AsStringView(),
            public_credential.metadata_encryption_key_tag());
  EXPECT_FALSE(
      public_credential.connection_signature_verification_key().empty());
  EXPECT_FALSE(public_credential.encrypted_metadata_bytes().empty());

  // Decrypt the device metadata

  auto decrypted_metadata = credential_manager_.DecryptMetadata(
      private_credential.metadata_encryption_key(),
      public_credential.key_seed(),
      public_credential.encrypted_metadata_bytes());

  EXPECT_EQ(metadata.SerializeAsString(), decrypted_metadata);
}

TEST_F(CredentialManagerImplTest, GenerateCredentialsSuccessfully) {
  constexpr int kLifeCycleDays = 1;
  constexpr int kNumCredentials = 5;
  Metadata metadata = CreateTestMetadata();
  absl::StatusOr<std::vector<SharedCredential>> public_credentials;
  std::vector<IdentityType> identityTypes{IDENTITY_TYPE_PRIVATE};
  absl::Time previous_start_time;
  absl::Time previous_end_time;

  credential_manager_.GenerateCredentials(
      metadata, kManagerAppId, identityTypes, kLifeCycleDays, kNumCredentials,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});

  EXPECT_OK(public_credentials);
  EXPECT_EQ(public_credentials->size(), kNumCredentials);
  for (int i = 0; i < kNumCredentials; i++) {
    SharedCredential& public_credential = public_credentials->at(i);
    EXPECT_EQ(public_credential.identity_type(), IDENTITY_TYPE_PRIVATE);
    EXPECT_FALSE(public_credential.secret_id().empty());
    absl::Time start_time_millis =
        absl::FromUnixMillis(public_credential.start_time_millis());
    absl::Time end_time_millis =
        absl::FromUnixMillis(public_credential.end_time_millis());
    if (i > 0) {
      EXPECT_GT(start_time_millis, previous_start_time);
      EXPECT_GE(previous_end_time, start_time_millis);
      EXPECT_GT(end_time_millis, previous_end_time);
    }
    EXPECT_LT(start_time_millis + absl::Hours(24) * kLifeCycleDays,
              end_time_millis);
    EXPECT_FALSE(public_credential.encrypted_metadata_bytes().empty());
    previous_start_time = start_time_millis;
    previous_end_time = end_time_millis;
  }
}

TEST_F(CredentialManagerImplTest,
       SubscribeCallsCallbackWithExistingCredentials) {
  absl::StatusOr<std::vector<SharedCredential>> public_credentials1;
  absl::StatusOr<std::vector<SharedCredential>> public_credentials2;
  AddLocalIdentity(kManagerAppId, kAccountName, IDENTITY_TYPE_PRIVATE);

  SubscriberId id1 = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{.manager_app_id = std::string(kManagerAppId),
                         .account_name = std::string(kAccountName),
                         .identity_type = IDENTITY_TYPE_PRIVATE},
      PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials1 = std::move(credentials);
           }});
  SubscriberId id2 = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{.manager_app_id = std::string(kManagerAppId),
                         .account_name = std::string(kAccountName),
                         .identity_type = IDENTITY_TYPE_PRIVATE},
      PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
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
  absl::StatusOr<std::vector<SharedCredential>> public_credentials;

  SubscriberId id = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{.manager_app_id = std::string(kManagerAppId),
                         .account_name = std::string(kAccountName),
                         .identity_type = IDENTITY_TYPE_PRIVATE},
      PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
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
  absl::StatusOr<std::vector<SharedCredential>> public_credentials;
  SubscriberId id = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{.manager_app_id = std::string(kManagerAppId),
                         .account_name = std::string(kAccountName),
                         .identity_type = IDENTITY_TYPE_PRIVATE},
      PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});

  credential_manager_.UnsubscribeFromPublicCredentials(id);
  AddLocalIdentity(kManagerAppId, kAccountName, IDENTITY_TYPE_PRIVATE);

  Fence();
  EXPECT_THAT(public_credentials, StatusIs(absl::StatusCode::kUnknown));
}

TEST_F(CredentialManagerImplTest,
       GenerateCredentialsSuccessfullyButStoreFailed) {
  Metadata metadata = CreateTestMetadata();
  auto credential_storage_ptr =
      std::make_unique<CredentialManagerImplTest::MockCredentialStorage>();
  EXPECT_CALL(*credential_storage_ptr, SaveCredentials)
      .WillOnce(::testing::Invoke(
          [](absl::string_view manager_app_id, absl::string_view account_name,
             const std::vector<LocalCredential>& private_credentials,
             const std::vector<SharedCredential>& public_credentials,
             PublicCredentialType public_credential_type,
             SaveCredentialsResultCallback callback) {
            callback.credentials_saved_cb(
                absl::FailedPreconditionError("Expected failure"));
          }));
  credential_manager_ =
      CredentialManagerImpl(&executor_, std::move(credential_storage_ptr));
  absl::StatusOr<std::vector<SharedCredential>> public_credentials;
  std::vector<IdentityType> identityTypes{IDENTITY_TYPE_PRIVATE};

  credential_manager_.GenerateCredentials(
      metadata, kManagerAppId, identityTypes, 1, 2,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});
  EXPECT_THAT(public_credentials,
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(CredentialManagerImplTest, UpdateRemotePublicCredentialsSuccessfully) {
  SharedCredential public_credential_for_test;
  public_credential_for_test.set_identity_type(
      IdentityType::IDENTITY_TYPE_TRUSTED);
  std::vector<SharedCredential> public_credentials{
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
  absl::StatusOr<std::vector<SharedCredential>> subscribed_credentials;
  SharedCredential public_credential_for_test;
  public_credential_for_test.set_identity_type(
      IdentityType::IDENTITY_TYPE_PRIVATE);
  std::vector<SharedCredential> public_credentials{
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
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             subscribed_credentials = std::move(credentials);
           }});
  SubscriberId id2 = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{.manager_app_id = std::string(kManagerAppId),
                         .account_name = std::string(kAccountName),
                         .identity_type = internal::IDENTITY_TYPE_TRUSTED},
      PublicCredentialType::kRemotePublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
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

TEST_F(CredentialManagerImplTest, GetLocalCredentialsFailed) {
  absl::StatusOr<std::vector<LocalCredential>> private_credentials;
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();

  credential_manager_.GetLocalCredentials(
      credential_selector,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<LocalCredential>> credentials) {
             private_credentials = std::move(credentials);
           }});

  EXPECT_THAT(private_credentials, StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CredentialManagerImplTest, GetPublicCredentialsFailed) {
  absl::StatusOr<std::vector<SharedCredential>> public_credentials;
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();

  credential_manager_.GetPublicCredentials(
      credential_selector, PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});

  EXPECT_THAT(public_credentials, StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CredentialManagerImplTest, GetCredentialsSuccessfully) {
  Metadata metadata = CreateTestMetadata();
  absl::StatusOr<std::vector<SharedCredential>> public_credentials;
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE};
  absl::StatusOr<std::vector<LocalCredential>> private_credentials;
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();

  credential_manager_.GenerateCredentials(
      metadata, kManagerAppId, identity_types, 1, 1,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});
  credential_manager_.GetLocalCredentials(
      credential_selector,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<LocalCredential>> credentials) {
             private_credentials = std::move(credentials);
           }});

  EXPECT_OK(public_credentials);
  EXPECT_EQ(public_credentials->size(), 1);
  EXPECT_OK(private_credentials);
  EXPECT_FALSE(private_credentials->empty());
}

TEST_F(CredentialManagerImplTest, PublicCredentialsFailEncryption) {
  Metadata metadata = CreateTestMetadata();
  absl::StatusOr<std::vector<SharedCredential>> public_credentials;
  auto credential_manager_ptr =
      std::make_unique<CredentialManagerImplTest::MockCredentialManager>(
          &executor_);
  EXPECT_CALL(*credential_manager_ptr, EncryptMetadata)
      .WillOnce(::testing::Invoke(
          [](absl::string_view metadata_encryption_key,
             absl::string_view key_seed,
             absl::string_view metadata_string) { return ""; }));
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE};

  credential_manager_ptr->GenerateCredentials(
      metadata, kManagerAppId, identity_types, 1, 1,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});

  EXPECT_THAT(public_credentials, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(CredentialManagerImplTest, UpdateLocalCredential) {
  constexpr int kNumCredentials = 5;
  constexpr int kSelectedCredentialId = 2;
  constexpr uint16_t kSalt = 1000;
  absl::Status update_status = absl::UnknownError("");
  Metadata metadata = CreateTestMetadata();
  absl::StatusOr<std::vector<nearby::internal::SharedCredential>>
      public_credentials;
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE,
                                           IDENTITY_TYPE_TRUSTED};
  absl::StatusOr<std::vector<LocalCredential>> private_credentials;
  absl::StatusOr<std::vector<LocalCredential>> modified_private_credentials;
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();
  credential_manager_.GenerateCredentials(
      metadata, kManagerAppId, identity_types, 1, kNumCredentials,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<nearby::internal::SharedCredential>>
                   credentials) {
             public_credentials = std::move(credentials);
           }});
  credential_manager_.GetLocalCredentials(
      credential_selector,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<LocalCredential>> credentials) {
             private_credentials = std::move(credentials);
           }});
  ASSERT_OK(public_credentials);
  ASSERT_OK(private_credentials);
  EXPECT_EQ(private_credentials->size(), kNumCredentials);

  // Modify a private credential
  LocalCredential& credential = private_credentials->at(kSelectedCredentialId);
  credential.mutable_consumed_salts()->insert({kSalt, true});

  credential_manager_.UpdateLocalCredential(
      credential_selector, credential,
      {[&](absl::Status status) { update_status = status; }});

  EXPECT_OK(update_status);

  // verify modified content
  credential_manager_.GetLocalCredentials(
      credential_selector,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<LocalCredential>> credentials) {
             modified_private_credentials = std::move(credentials);
           }});
  ASSERT_OK(modified_private_credentials);
  EXPECT_THAT(*modified_private_credentials,
              UnorderedPointwise(EqualsProto(), *private_credentials));
}

}  // namespace

}  // namespace presence
}  // namespace nearby
