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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "net/proto2/contrib/parse_proto/testing.h"
#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
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

using ::nearby::internal::DeviceIdentityMetaData;
using ::nearby::internal::SharedCredential;
using ::nearby::internal::IdentityType::IDENTITY_TYPE_CONTACTS_GROUP;
using ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP;
using ::protobuf_matchers::EqualsProto;
using ::testing::UnorderedPointwise;
using ::testing::status::StatusIs;

constexpr absl::string_view kManagerAppId = "TEST_MANAGER_APP";
constexpr absl::string_view kAccountName = "";
constexpr int kExpectedPresenceCredentialListSize = 6;
constexpr int kExpectedPresenceCredentialValidDays = 5;

DeviceIdentityMetaData CreateTestDeviceIdentityMetaData() {
  DeviceIdentityMetaData device_identity_metadata;
  device_identity_metadata.set_device_type(
      internal::DeviceType::DEVICE_TYPE_PHONE);
  device_identity_metadata.set_device_name("NP test device");
  device_identity_metadata.set_bluetooth_mac_address("FF:FF:FF:FF:FF:FF");
  device_identity_metadata.set_device_id("\x12\xab\xcd");
  return device_identity_metadata;
}

CredentialSelector BuildDefaultCredentialSelector() {
  CredentialSelector credential_selector;
  credential_selector.manager_app_id = std::string(kManagerAppId);
  credential_selector.account_name = std::string(kAccountName);
  credential_selector.identity_type = IDENTITY_TYPE_PRIVATE_GROUP;
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

  class FakeCredentialStorage : public nearby::CredentialStorageImpl {
   public:
    // nearby::CredentialStorageImpl:
    void SaveCredentials(
        absl::string_view manager_app_id, absl::string_view account_name,
        const std::vector<LocalCredential>& private_credentials,
        const std::vector<SharedCredential>& public_credentials,
        PublicCredentialType public_credential_type,
        SaveCredentialsResultCallback callback) override {
      // Capture the credentials before actually saving them, so that they
      // can be manipulated later on.
      private_credentials_ = private_credentials;
      public_credentials_ = public_credentials;

      nearby::CredentialStorageImpl::SaveCredentials(
          manager_app_id, account_name, private_credentials, public_credentials,
          public_credential_type, std::move(callback));
    }
    void GetLocalCredentials(
        const CredentialSelector& credential_selector,
        GetLocalCredentialsResultCallback callback) override {
      if (private_credentials_.has_value()) {
        callback.credentials_fetched_cb(private_credentials_.value());
      } else {
        nearby::CredentialStorageImpl::GetLocalCredentials(credential_selector,
                                                           std::move(callback));
      }
    }
    void GetPublicCredentials(
        const CredentialSelector& credential_selector,
        PublicCredentialType public_credential_type,
        GetPublicCredentialsResultCallback callback) override {
      if (public_credentials_.has_value()) {
        callback.credentials_fetched_cb(public_credentials_.value());
      } else {
        nearby::CredentialStorageImpl::GetPublicCredentials(
            credential_selector, public_credential_type, std::move(callback));
      }
    }

    std::optional<std::vector<::nearby::internal::SharedCredential>>
        public_credentials_;
    std::optional<std::vector<::nearby::internal::LocalCredential>>
        private_credentials_;
  };

  class MockCredentialManager : public CredentialManagerImpl {
   public:
    explicit MockCredentialManager(SingleThreadExecutor* executor)
        : CredentialManagerImpl(executor) {}
    MOCK_METHOD(std::string, EncryptDeviceIdentityMetaData,
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
    auto public_credentials = GenerateCredentialsSync(
        CreateTestDeviceIdentityMetaData(), manager_app_id, {identity_type},
        /*credential_life_cycle_days=*/kExpectedPresenceCredentialValidDays,
        /*contiguous_copy_of_credentials=*/1);
    EXPECT_OK(public_credentials);
  }

  absl::StatusOr<std::vector<SharedCredential>> GenerateCredentialsSync(
      const DeviceIdentityMetaData& device_identity_metadata,
      absl::string_view manager_app_id,
      const std::vector<IdentityType>& identity_types,
      int credential_life_cycle_days, int contiguous_copy_of_credentials) {
    absl::StatusOr<std::vector<SharedCredential>> public_credentials;

    CountDownLatch latch(1);
    credential_manager_.GenerateCredentials(
        device_identity_metadata, manager_app_id, identity_types,
        credential_life_cycle_days, contiguous_copy_of_credentials,
        {.credentials_generated_cb =
             [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
               public_credentials = credentials;
               latch.CountDown();
             }});
    EXPECT_TRUE(latch.Await().Ok());

    return public_credentials;
  }

  std::vector<LocalCredential> GetLocalCredentialsSync(
      CredentialSelector credential_selector) {
    auto private_credentials = credential_manager_.GetLocalCredentialsSync(
        credential_selector, absl::Seconds(1));
    EXPECT_TRUE(private_credentials.ok());
    return private_credentials.GetResult();
  }

  std::vector<SharedCredential> GetPublicCredentialsSync(
      CredentialSelector credential_selector,
      PublicCredentialType public_credential_type) {
    auto public_credentials = credential_manager_.GetPublicCredentialsSync(
        credential_selector, public_credential_type, absl::Seconds(1));
    EXPECT_TRUE(public_credentials.ok());
    return public_credentials.GetResult();
  }

 protected:
  SingleThreadExecutor executor_;
  CredentialManagerImpl credential_manager_{&executor_};
  MockCredentialManager mock_credential_manager_{&executor_};
};

TEST_F(CredentialManagerImplTest, CreateOneCredentialSuccessfully) {
  auto device_identity_metadata = CreateTestDeviceIdentityMetaData();
  constexpr absl::Time kStartTime = absl::FromUnixSeconds(100000);
  constexpr absl::Time kEndTime = absl::FromUnixSeconds(200000);

  auto credentials = credential_manager_.CreateLocalCredential(
      device_identity_metadata, IDENTITY_TYPE_PRIVATE_GROUP, kStartTime,
      kEndTime);

  LocalCredential private_credential = credentials.first;
  // Verify the private credential.
  EXPECT_EQ(private_credential.identity_type(), IDENTITY_TYPE_PRIVATE_GROUP);
  EXPECT_NE(private_credential.id(), 0);
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
  EXPECT_EQ(public_credential.identity_type(), IDENTITY_TYPE_PRIVATE_GROUP);
  EXPECT_NE(public_credential.id(), 0);
  EXPECT_EQ(private_credential.id(), public_credential.id());
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

  auto decrypted_metadata = credential_manager_.DecryptDeviceIdentityMetaData(
      private_credential.metadata_encryption_key_v0(),
      public_credential.key_seed(),
      public_credential.encrypted_metadata_bytes_v0());

  EXPECT_EQ(device_identity_metadata.SerializeAsString(), decrypted_metadata);
}

TEST_F(CredentialManagerImplTest, GenerateCredentialsSuccessfully) {
  auto device_identity_metadata = CreateTestDeviceIdentityMetaData();
  std::vector<IdentityType> identityTypes{IDENTITY_TYPE_PRIVATE_GROUP};
  absl::Time previous_start_time;
  absl::Time previous_end_time;

  auto public_credentials = GenerateCredentialsSync(
      device_identity_metadata, kManagerAppId, identityTypes,
      kExpectedPresenceCredentialValidDays,
      kExpectedPresenceCredentialListSize);
  EXPECT_OK(public_credentials);
  EXPECT_EQ(public_credentials->size(), kExpectedPresenceCredentialListSize);

  for (int i = 0; i < kExpectedPresenceCredentialListSize; i++) {
    SharedCredential& public_credential = public_credentials->at(i);
    EXPECT_EQ(public_credential.identity_type(), IDENTITY_TYPE_PRIVATE_GROUP);
    EXPECT_NE(public_credential.id(), 0);
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
  AddLocalIdentity(kManagerAppId, kAccountName, IDENTITY_TYPE_PRIVATE_GROUP);

  SubscriberId id1 = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{.manager_app_id = std::string(kManagerAppId),
                         .account_name = std::string(kAccountName),
                         .identity_type = IDENTITY_TYPE_PRIVATE_GROUP},
      PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials1 = std::move(credentials);
           }});
  SubscriberId id2 = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{.manager_app_id = std::string(kManagerAppId),
                         .account_name = std::string(kAccountName),
                         .identity_type = IDENTITY_TYPE_PRIVATE_GROUP},
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
                         .identity_type = IDENTITY_TYPE_PRIVATE_GROUP},
      PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});
  Fence();
  EXPECT_THAT(public_credentials, StatusIs(absl::StatusCode::kUnknown));

  AddLocalIdentity(kManagerAppId, kAccountName, IDENTITY_TYPE_PRIVATE_GROUP);

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
                         .identity_type = IDENTITY_TYPE_PRIVATE_GROUP},
      PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
           }});

  credential_manager_.UnsubscribeFromPublicCredentials(id);
  AddLocalIdentity(kManagerAppId, kAccountName, IDENTITY_TYPE_PRIVATE_GROUP);

  Fence();
  EXPECT_THAT(public_credentials, StatusIs(absl::StatusCode::kUnknown));
}

TEST_F(CredentialManagerImplTest,
       GenerateCredentialsSuccessfullyButStoreFailed) {
  auto device_identity_metadata = CreateTestDeviceIdentityMetaData();
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
  std::vector<IdentityType> identityTypes{IDENTITY_TYPE_PRIVATE_GROUP};

  auto public_credentials = GenerateCredentialsSync(
      device_identity_metadata, kManagerAppId, identityTypes,
      kExpectedPresenceCredentialValidDays,
      kExpectedPresenceCredentialListSize);
  EXPECT_THAT(public_credentials,
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(CredentialManagerImplTest, UpdateRemotePublicCredentialsSuccessfully) {
  SharedCredential public_credential_for_test;
  public_credential_for_test.set_identity_type(
      IdentityType::IDENTITY_TYPE_CONTACTS_GROUP);
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
      IdentityType::IDENTITY_TYPE_PRIVATE_GROUP);
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
      CredentialSelector{
          .manager_app_id = std::string(kManagerAppId),
          .account_name = std::string(kAccountName),
          .identity_type = internal::IDENTITY_TYPE_PRIVATE_GROUP},
      PublicCredentialType::kRemotePublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             subscribed_credentials = std::move(credentials);
           }});
  SubscriberId id2 = credential_manager_.SubscribeForPublicCredentials(
      CredentialSelector{
          .manager_app_id = std::string(kManagerAppId),
          .account_name = std::string(kAccountName),
          .identity_type = internal::IDENTITY_TYPE_CONTACTS_GROUP},
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

  CountDownLatch latch(1);
  credential_manager_.GetPublicCredentials(
      credential_selector, PublicCredentialType::kLocalPublicCredential,
      {.credentials_fetched_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
             latch.CountDown();
           }});
  EXPECT_TRUE(latch.Await().Ok());

  EXPECT_THAT(public_credentials, StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CredentialManagerImplTest, GetCredentialsSuccessfully) {
  auto device_identity_metadata = CreateTestDeviceIdentityMetaData();
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE_GROUP};
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();

  auto public_credentials = GenerateCredentialsSync(
      device_identity_metadata, kManagerAppId, identity_types,
      kExpectedPresenceCredentialValidDays,
      kExpectedPresenceCredentialListSize);
  EXPECT_OK(public_credentials);
  EXPECT_EQ(public_credentials->size(), kExpectedPresenceCredentialListSize);

  auto private_credentials = GetLocalCredentialsSync(credential_selector);
  EXPECT_FALSE(private_credentials.empty());
}

TEST_F(CredentialManagerImplTest, PublicCredentialsFailEncryption) {
  auto device_identity_metadata = CreateTestDeviceIdentityMetaData();
  absl::StatusOr<std::vector<SharedCredential>> public_credentials;
  auto credential_manager_ptr =
      std::make_unique<CredentialManagerImplTest::MockCredentialManager>(
          &executor_);
  EXPECT_CALL(*credential_manager_ptr, EncryptDeviceIdentityMetaData)
      .WillOnce(::testing::Invoke(
          [](absl::string_view metadata_encryption_key,
             absl::string_view key_seed,
             absl::string_view metadata_string) { return ""; }));
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE_GROUP};

  CountDownLatch latch(1);
  credential_manager_ptr->GenerateCredentials(
      device_identity_metadata, kManagerAppId, identity_types,
      kExpectedPresenceCredentialValidDays, 1,
      {.credentials_generated_cb =
           [&](absl::StatusOr<std::vector<SharedCredential>> credentials) {
             public_credentials = std::move(credentials);
             latch.CountDown();
           }});
  EXPECT_TRUE(latch.Await().Ok());

  EXPECT_THAT(public_credentials, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(CredentialManagerImplTest, UpdateLocalCredential) {
  constexpr int kSelectedCredentialId = 2;
  constexpr uint16_t kSalt = 1000;
  absl::Status update_status = absl::UnknownError("");
  auto device_identity_metadata = CreateTestDeviceIdentityMetaData();
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE_GROUP,
                                           IDENTITY_TYPE_CONTACTS_GROUP};
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();
  auto public_credentials = GenerateCredentialsSync(
      device_identity_metadata, kManagerAppId, identity_types,
      kExpectedPresenceCredentialValidDays,
      kExpectedPresenceCredentialListSize);

  auto private_credentials = GetLocalCredentialsSync(credential_selector);
  EXPECT_EQ(kExpectedPresenceCredentialListSize, private_credentials.size());

  ASSERT_OK(public_credentials);

  // Modify a private credential
  auto credential = private_credentials.at(kSelectedCredentialId);
  EXPECT_TRUE(
      private_credentials.at(kSelectedCredentialId).consumed_salts().empty());
  credential.mutable_consumed_salts()->insert({kSalt, true});

  credential_manager_.UpdateLocalCredential(
      credential_selector, credential,
      {[&](absl::Status status) { update_status = status; }});

  EXPECT_OK(update_status);

  // Verify that the modified credential has the new field in the new
  // retrieved list of credentials.
  auto modified_private_credentials =
      GetLocalCredentialsSync(credential_selector);
  EXPECT_TRUE(modified_private_credentials.at(kSelectedCredentialId)
                  .consumed_salts()
                  .at(kSalt));
}

TEST_F(CredentialManagerImplTest, EncryptAndDecryptDeviceIdentityMetaData) {
  constexpr absl::string_view kMetadataEncryptionKeyBase16 =
      "6331578C6E244074111B2ED0BBDB";
  constexpr absl::string_view kSeed = "123456";

  auto encrypted_meta_data = credential_manager_.EncryptDeviceIdentityMetaData(
      kMetadataEncryptionKeyBase16, kSeed,
      CreateTestDeviceIdentityMetaData().SerializeAsString());

  auto decrypted_meta_data = credential_manager_.DecryptDeviceIdentityMetaData(
      kMetadataEncryptionKeyBase16, kSeed, encrypted_meta_data);

  DeviceIdentityMetaData device_identity_metadata;
  ASSERT_TRUE(device_identity_metadata.ParseFromString(decrypted_meta_data));
  EXPECT_EQ(device_identity_metadata.device_id(), "\x12\xab\xcd");
  EXPECT_EQ(device_identity_metadata.device_type(),
            internal::DeviceType::DEVICE_TYPE_PHONE);
  EXPECT_EQ(device_identity_metadata.device_name(), "NP test device");
  EXPECT_EQ(device_identity_metadata.bluetooth_mac_address(),
            "FF:FF:FF:FF:FF:FF");
}

TEST_F(CredentialManagerImplTest, RefillCredentialsInGetLocalCredentials) {
  auto device_identity_metadata = CreateTestDeviceIdentityMetaData();
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE_GROUP};
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();

  auto public_credentials = GenerateCredentialsSync(
      device_identity_metadata, kManagerAppId, identity_types,
      kExpectedPresenceCredentialValidDays, 1);

  EXPECT_OK(public_credentials);
  EXPECT_EQ(1, public_credentials->size());

  // only generate 1 creds, expecting GetLocal would trigger refill to
  // kExpectedPresenceCredentialListSize.
  auto private_credentials = GetLocalCredentialsSync(credential_selector);
  EXPECT_EQ(kExpectedPresenceCredentialListSize, private_credentials.size());
}

TEST_F(CredentialManagerImplTest, RefillCredentialsInGetSharedCredentials) {
  auto device_identity_metadata = CreateTestDeviceIdentityMetaData();
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE_GROUP};
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();

  auto public_credentials = GenerateCredentialsSync(
      device_identity_metadata, kManagerAppId, identity_types,
      kExpectedPresenceCredentialValidDays, 1);
  EXPECT_OK(public_credentials);
  EXPECT_EQ(1, public_credentials->size());

  // Only generated 1 creds, expecting GetPublicCredentials for
  // kLocalPublicCredential type would trigger refill to
  // kExpectedPresenceCredentialListSize.
  auto refilled_public_credentials = GetPublicCredentialsSync(
      credential_selector, PublicCredentialType::kLocalPublicCredential);
  EXPECT_EQ(kExpectedPresenceCredentialListSize,
            refilled_public_credentials.size());
}

TEST_F(CredentialManagerImplTest, RefillExpiredCredsInGetLocal) {
  auto device_identity_metadata = CreateTestDeviceIdentityMetaData();
  std::vector<IdentityType> identity_types{IDENTITY_TYPE_PRIVATE_GROUP};
  CredentialSelector credential_selector = BuildDefaultCredentialSelector();

  auto credential_storage =
      std::make_unique<CredentialManagerImplTest::FakeCredentialStorage>();
  auto* credential_storage_ptr = credential_storage.get();
  credential_manager_ =
      CredentialManagerImpl(&executor_, std::move(credential_storage));

  auto public_credentials = GenerateCredentialsSync(
      device_identity_metadata, kManagerAppId, identity_types,
      kExpectedPresenceCredentialValidDays,
      kExpectedPresenceCredentialListSize);

  ASSERT_OK(public_credentials);
  EXPECT_EQ(public_credentials->size(), kExpectedPresenceCredentialListSize);

  // Now that we have generated kExpectedPresenceCredentialListSize valid creds,
  // tweak the first credential's end time, in both credential lists, to
  // make them expired.
  auto expiry_time = absl::ToUnixMillis(absl::Now() - absl::Hours(1));
  credential_storage_ptr->private_credentials_.value()
      .at(0)
      .set_end_time_millis(expiry_time);
  credential_storage_ptr->public_credentials_.value().at(0).set_end_time_millis(
      expiry_time);

  auto old_private_credentials =
      credential_storage_ptr->private_credentials_.value();

  auto refilled_private_credentials =
      GetLocalCredentialsSync(credential_selector);
  EXPECT_EQ(kExpectedPresenceCredentialListSize,
            refilled_private_credentials.size());

  // Verifying the expired one private_credentials->at(0) is pruned in the new
  // list.
  EXPECT_EQ(old_private_credentials.at(1).secret_id(),
            refilled_private_credentials.at(0).secret_id());
  // Verifying the new generated cred's start time is the same as previously
  // existing list's last cred's end time.
  EXPECT_EQ(
      old_private_credentials.at(5).end_time_millis(),
      refilled_private_credentials.at(kExpectedPresenceCredentialListSize - 1)
          .start_time_millis());
}

}  // namespace

}  // namespace presence
}  // namespace nearby
