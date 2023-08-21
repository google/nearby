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
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/credential_storage_impl.h"
#include "internal/platform/implementation/credential_callbacks.h"
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
constexpr int kExpectedPresenceCredentialListSize = 6;
constexpr int kExpectedPresenceCredentialValidDays = 5;

Metadata CreateTestMetadata(absl::string_view account_name = kAccountName) {
  Metadata metadata;
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
        /*credential_life_cycle_days=*/kExpectedPresenceCredentialValidDays,
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
  EXPECT_EQ(private_credential.metadata_encryption_key_v0().size(),
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
  EXPECT_EQ(Crypto::Sha256(private_credential.metadata_encryption_key_v0())
                .AsStringView(),
            public_credential.metadata_encryption_key_tag_v0());
  EXPECT_FALSE(
      public_credential.connection_signature_verification_key().empty());
  EXPECT_FALSE(public_credential.encrypted_metadata_bytes_v0().empty());

  // Decrypt the device metadata

  auto decrypted_metadata = credential_manager_.DecryptMetadata(
      private_credential.metadata_encryption_key_v0(),
      public_credential.key_seed(),
      public_credential.encrypted_metadata_bytes_v0());

  EXPECT_EQ(metadata.SerializeAsString(), decrypted_metadata);
}

TEST_F(CredentialManagerImplTest, GenerateCredentialsSuccessfully) {
  Metadata metadata = CreateTestMetadata();
  absl::StatusOr<std::vector<SharedCredential>> public_credentials;
  std::vector<IdentityType> identityTypes{IDENTITY_TYPE_PRIVATE};
  absl::Time previous_start_time;
  absl::Time previous_end_time;

  credential_manager_.GenerateCredentials(
      metadata, kManagerAppId, identityTypes,
      kExpectedPresenceCredentialValidDays, kExpectedPresenceCredentialListSize,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});

  EXPECT_OK(public_credentials);
  EXPECT_EQ(public_credentials->size(), kExpectedPresenceCredentialListSize);
  for (int i = 0; i < kExpectedPresenceCredentialListSize; i++) {
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
    EXPECT_LT(start_time_millis +
                  absl::Hours(24) * kExpectedPresenceCredentialValidDays,
              end_time_millis);
    EXPECT_FALSE(public_credential.encrypted_metadata_bytes_v0().empty());
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
  EXPECT_EQ(public_credentials1->size(), kExpectedPresenceCredentialListSize);
  EXPECT_EQ(public_credentials2->size(), kExpectedPresenceCredentialListSize);
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
  EXPECT_EQ(public_credentials->size(), kExpectedPresenceCredentialListSize);
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
      metadata, kManagerAppId, identityTypes,
      kExpectedPresenceCredentialValidDays, kExpectedPresenceCredentialListSize,
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
      metadata, kManagerAppId, identity_types,
      kExpectedPresenceCredentialValidDays, kExpectedPresenceCredentialListSize,
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
  EXPECT_EQ(public_credentials->size(), kExpectedPresenceCredentialListSize);
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
      metadata, kManagerAppId, identity_types,
      kExpectedPresenceCredentialValidDays, 1,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});

  EXPECT_THAT(public_credentials, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(CredentialManagerImplTest, UpdateLocalCredential) {
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
      metadata, kManagerAppId, identity_types,
      kExpectedPresenceCredentialValidDays, kExpectedPresenceCredentialListSize,
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
  EXPECT_EQ(private_credentials->size(), kExpectedPresenceCredentialListSize);

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

TEST_F(CredentialManagerImplTest, ParseAndroidSharedCredential) {
  // This SharedCredential and Metadata were generated on Android.
  constexpr absl::string_view kSharedCredentialBase16 =
      "0A20C8B6DB66CBA77E8CF0286A78574D1F7EADF3C3DAA3E26DAB048FD5481B4FBA7A1220"
      "E809E7805FC6AB8226A4CCA9FAEA5FDDE49EE07E7D5905CCB6AA0F779069F2A818D088D4"
      "EADE3020B093BEBDE0302A56E1CAE889FC26B8FBF399C86BEB8D7AB84EE476EF2E75B465"
      "773A957BDEAD6732FCD74BFFC363BE068CCD9109108AAE5274C861675F3E7E0E524C4A75"
      "11DC9F669FCAC072EE70062B00AEA4A736454FC1CAAE6F8D62843220D563DF856310428E"
      "FE6D8B6FBD74FB9C4E762323782494D0E2DF6FEA118E17B13A5B3059301306072A8648CE"
      "3D020106082A8648CE3D03010703420004169A965ACFCAE31B031147A0169823B4B6926D"
      "7AA86E50CABB5F6100F6992D5C1769FC629F0F789B7B39525DA4A33FC2438A074DC1EEC0"
      "21B1AA4FD2122DC5044801";
  constexpr absl::string_view kMetadataEncryptionKeyBase16 =
      "6331578C6E244074111B2ED0BBDB";
  constexpr absl::string_view kMetadataBase16 =
      "08011A137363616E6E657220646576696365206E616D6522137363616E6E657220706572"
      "736F6E206E616D652A107363616E6E65722069636F6E2075726C3206AABBCCDDEEFF";
  SharedCredential shared_credential;
  Metadata expected_metadata;

  ASSERT_TRUE(shared_credential.ParseFromString(
      absl::HexStringToBytes(kSharedCredentialBase16)));
  ASSERT_TRUE(expected_metadata.ParseFromString(
      absl::HexStringToBytes(kMetadataBase16)));
  std::string decrypted_metadata = credential_manager_.DecryptMetadata(
      absl::HexStringToBytes(kMetadataEncryptionKeyBase16),
      shared_credential.key_seed(),
      shared_credential.encrypted_metadata_bytes_v0());
  Metadata metadata;
  ASSERT_TRUE(metadata.ParseFromString(decrypted_metadata));
  EXPECT_THAT(metadata, EqualsProto(expected_metadata));
}

TEST_F(CredentialManagerImplTest, RefillCredentailInGetLocalCredentials) {
  Metadata metadata = CreateTestMetadata();
  absl::StatusOr<std::vector<SharedCredential>> public_credentials;
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE};
  absl::StatusOr<std::vector<LocalCredential>> private_credentials;
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();

  credential_manager_.GenerateCredentials(
      metadata, kManagerAppId, identity_types,
      kExpectedPresenceCredentialValidDays, 1,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});

  EXPECT_OK(public_credentials);
  EXPECT_EQ(public_credentials->size(), 1);

  // only generate 1 creds, expecting GetLocal would trigger refill to
  // kExpectedPresenceCredentialListSize.
  credential_manager_.GetLocalCredentials(
      credential_selector,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<LocalCredential>> credentials) {
             private_credentials = std::move(credentials);
           }});

  EXPECT_OK(private_credentials);
  EXPECT_EQ(private_credentials->size(), kExpectedPresenceCredentialListSize);
}

TEST_F(CredentialManagerImplTest, RefillCredentailInGetSharedCredentials) {
  Metadata metadata = CreateTestMetadata();
  absl::StatusOr<std::vector<SharedCredential>> public_credentials;
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE};
  absl::StatusOr<std::vector<SharedCredential>> refilled_public_credentials;
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();

  credential_manager_.GenerateCredentials(
      metadata, kManagerAppId, identity_types,
      kExpectedPresenceCredentialValidDays, 1,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});

  EXPECT_OK(public_credentials);
  EXPECT_EQ(public_credentials->size(), 1);

  // Only generated 1 creds, expecting GetPublicCredentials for
  // kLocalPublicCredential type would trigger refill to
  // kExpectedPresenceCredentialListSize.
  credential_manager_.GetPublicCredentials(
      credential_selector, PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             refilled_public_credentials = std::move(credentials);
           }});

  EXPECT_OK(refilled_public_credentials);
  EXPECT_EQ(refilled_public_credentials->size(),
            kExpectedPresenceCredentialListSize);
}

TEST_F(CredentialManagerImplTest, RefillExpiredCredsInGetLocal) {
  Metadata metadata = CreateTestMetadata();
  absl::StatusOr<std::vector<SharedCredential>> public_credentials;
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE};
  absl::StatusOr<std::vector<LocalCredential>> private_credentials;
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();

  credential_manager_.GenerateCredentials(
      metadata, kManagerAppId, identity_types,
      kExpectedPresenceCredentialValidDays, kExpectedPresenceCredentialListSize,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});

  ASSERT_OK(public_credentials);
  EXPECT_EQ(public_credentials->size(), kExpectedPresenceCredentialListSize);

  // Now generated kExpectedPresenceCredentialListSize valid creds, read out the
  // local creds list, then manually update the first credential's end time to
  // make it expired.
  credential_manager_.GetLocalCredentials(
      credential_selector,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<LocalCredential>> credentials) {
             private_credentials = std::move(credentials);
           }});

  ASSERT_OK(private_credentials);
  EXPECT_EQ(private_credentials->size(), kExpectedPresenceCredentialListSize);

  LocalCredential expiring_local_credential = private_credentials->at(0);

  expiring_local_credential.set_end_time_millis(
      absl::ToUnixMillis(absl::Now() - absl::Hours(1)));

  CountDownLatch update_local_cred_latch(1);

  credential_manager_.UpdateLocalCredential(
      credential_selector, expiring_local_credential,
      {
          .credentials_saved_cb =
              [&](absl::Status status) {
                if (status.ok()) {
                  update_local_cred_latch.CountDown();
                }
              },
      });
  EXPECT_TRUE(update_local_cred_latch.Await().Ok());

  absl::StatusOr<std::vector<LocalCredential>> refilled_private_credentials;
  credential_manager_.GetLocalCredentials(
      credential_selector,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<LocalCredential>> credentials) {
             refilled_private_credentials = std::move(credentials);
           }});

  EXPECT_OK(refilled_private_credentials);
  EXPECT_EQ(refilled_private_credentials->size(),
            kExpectedPresenceCredentialListSize);
  // Verifying the expired one private_credentials->at(0) is pruned in the new
  // list.
  EXPECT_EQ(private_credentials->at(1).secret_id(),
            refilled_private_credentials->at(0).secret_id());
  // Verifying the new generated cred's start time is the same as previously
  // exisiting list's last cred's end time.
  EXPECT_EQ(
      private_credentials->at(5).end_time_millis(),
      refilled_private_credentials->at(kExpectedPresenceCredentialListSize - 1)
          .start_time_millis());
}

}  // namespace

}  // namespace presence
}  // namespace nearby
