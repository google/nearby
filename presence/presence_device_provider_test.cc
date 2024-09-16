// Copyright 2023 Google LLC
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

#include "presence/presence_device_provider.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/crypto/ed25519.h"
#include "internal/interop/authentication_status.h"
#include "internal/interop/authentication_transport.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/local_credential.pb.h"
#include "internal/proto/metadata.pb.h"
#include "presence/implementation/connection_authenticator.h"
#include "presence/implementation/mock_connection_authenticator.h"
#include "presence/implementation/mock_service_controller.h"
#include "presence/presence_device.h"
#include "presence/proto/presence_frame.pb.h"

namespace nearby {
namespace presence {
namespace {
using ::nearby::internal::DeviceIdentityMetaData;

constexpr absl::string_view kMacAddr = "\x4C\x8B\x1D\xCE\xBA\xD1";
constexpr absl::string_view kManagerAppId = "test_app_id";
constexpr char kUkey2Secret[] = {0x34, 0x56, 0x78, 0x90};
constexpr char kKeySeed[] = {1, 2, 3, 4, 5, 6, 7, 8};
constexpr int kPresenceVersion = 1;
constexpr absl::string_view kSharedCredentialHash = "shared_cred_hash";
constexpr absl::string_view kPrivateKeySignature = "private_key_signature";

DeviceIdentityMetaData CreateTestDeviceIdentityMetaData() {
  DeviceIdentityMetaData device_identity_metadata;
  device_identity_metadata.set_device_type(
      internal::DeviceType::DEVICE_TYPE_PHONE);
  device_identity_metadata.set_device_name("NP test device");
  device_identity_metadata.set_bluetooth_mac_address(kMacAddr);
  device_identity_metadata.set_device_id("\x12\xab\xcd");
  return device_identity_metadata;
}

nearby::internal::LocalCredential CreateValidLocalCredential(
    const crypto::Ed25519KeyPair& key_pair) {
  nearby::internal::LocalCredential credential;
  absl::Time now = SystemClock::ElapsedRealtime();
  credential.set_start_time_millis(absl::ToUnixMillis(now));
  credential.set_end_time_millis(absl::ToUnixMillis(now + absl::Minutes(10)));
  credential.mutable_connection_signing_key()->set_key(
      absl::StrCat(key_pair.private_key, key_pair.public_key));
  credential.set_key_seed(kKeySeed);
  return credential;
}

nearby::internal::LocalCredential CreateExpiredLocalCredential() {
  nearby::internal::LocalCredential credential;
  absl::Time now = SystemClock::ElapsedRealtime();
  credential.set_start_time_millis(absl::ToUnixMillis(now - absl::Minutes(30)));
  credential.set_end_time_millis(absl::ToUnixMillis(now - absl::Minutes(10)));
  return credential;
}

internal::SharedCredential BuildSharedCredential(
    const crypto::Ed25519KeyPair& key_pair) {
  internal::SharedCredential shared_credential;
  shared_credential.set_connection_signature_verification_key(
      key_pair.public_key);
  shared_credential.set_key_seed(kKeySeed);
  return shared_credential;
}

ConnectionAuthenticator::TwoWayInitiatorData BuildDefaultInitiatorData() {
  ConnectionAuthenticator::TwoWayInitiatorData data;
  data.shared_credential_hash = kSharedCredentialHash;
  data.private_key_signature = kPrivateKeySignature;
  return data;
}

class MockAuthenticationTransport : public AuthenticationTransport {
 public:
  MOCK_METHOD(void, WriteMessage, (absl::string_view), (const, override));
  MOCK_METHOD(std::string, ReadMessage, (), (const, override));
};

class PresenceDeviceProviderTest : public ::testing::Test {
 public:
  PresenceDeviceProviderTest() {
    ON_CALL(mock_service_controller_, GetDeviceIdentityMetaData)
        .WillByDefault(testing::Return(CreateTestDeviceIdentityMetaData()));
    provider_ = std::make_unique<PresenceDeviceProvider>(
        &mock_service_controller_, &mock_connection_authenticator_);
  }

  void SetUp() override {
    auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
    ASSERT_OK_AND_ASSIGN(key_pair_, key_pair_or_status);
  }

 protected:
  MockServiceController mock_service_controller_;
  std::unique_ptr<PresenceDeviceProvider> provider_;
  crypto::Ed25519KeyPair key_pair_;
  MockConnectionAuthenticator mock_connection_authenticator_;
};

TEST_F(PresenceDeviceProviderTest, ProviderIsNotTriviallyConstructible) {
  EXPECT_FALSE(std::is_trivially_constructible<PresenceDeviceProvider>::value);
}

TEST_F(PresenceDeviceProviderTest, DeviceProviderWorks) {
  auto device = provider_->GetLocalDevice();
  ASSERT_EQ(device->GetType(), NearbyDevice::Type::kPresenceDevice);
  auto presence_device = static_cast<const PresenceDevice*>(device);
  EXPECT_EQ(presence_device->GetDeviceIdentityMetadata().SerializeAsString(),
            CreateTestDeviceIdentityMetaData().SerializeAsString());
}

TEST_F(PresenceDeviceProviderTest, DeviceProviderCanUpdateDevice) {
  auto device = provider_->GetLocalDevice();
  ASSERT_EQ(device->GetType(), NearbyDevice::Type::kPresenceDevice);
  auto presence_device = static_cast<const PresenceDevice*>(device);
  EXPECT_EQ(presence_device->GetDeviceIdentityMetadata().SerializeAsString(),
            CreateTestDeviceIdentityMetaData().SerializeAsString());
  auto new_metadata = CreateTestDeviceIdentityMetaData();
  new_metadata.set_device_name("NP interop device");
  provider_->UpdateDeviceIdentityMetaData(new_metadata);
  EXPECT_EQ(presence_device->GetDeviceIdentityMetadata().SerializeAsString(),
            new_metadata.SerializeAsString());
}

TEST_F(PresenceDeviceProviderTest, SetGetManagerAppId) {
  provider_->SetManagerAppId(kManagerAppId);
  EXPECT_EQ(provider_->GetManagerAppId(), kManagerAppId);
}

TEST_F(PresenceDeviceProviderTest,
       AuthenticateAsInitiatorFails_FailToFetchCredentials) {
  EXPECT_CALL(mock_service_controller_, GetLocalCredentials)
      .WillOnce([&](const CredentialSelector& credential_selector,
                    GetLocalCredentialsResultCallback callback) {
        std::move(callback.credentials_fetched_cb)(
            absl::Status(absl::StatusCode::kCancelled, /*msg=*/std::string()));
      });

  PresenceDevice remote_device(CreateTestDeviceIdentityMetaData());
  MockAuthenticationTransport authentication_transport;
  auto status = provider_->AuthenticateAsInitiator(
      /*remote_device=*/remote_device, /*shared_secret=*/kUkey2Secret,
      /*authentication_transport=*/authentication_transport);
  EXPECT_EQ(AuthenticationStatus::kFailure, status);
}

TEST_F(PresenceDeviceProviderTest,
       AuthenticateAsInitiatorFails_NoValidCredentials) {
  EXPECT_CALL(mock_service_controller_, GetLocalCredentials)
      .WillOnce([&](const CredentialSelector& credential_selector,
                    GetLocalCredentialsResultCallback callback) {
        std::vector<nearby::internal::LocalCredential> credentials;
        credentials.push_back(CreateExpiredLocalCredential());
        std::move(callback.credentials_fetched_cb)(credentials);
      });

  PresenceDevice remote_device(CreateTestDeviceIdentityMetaData());
  MockAuthenticationTransport authentication_transport;
  auto status = provider_->AuthenticateAsInitiator(
      /*remote_device=*/remote_device, /*shared_secret=*/kUkey2Secret,
      /*authentication_transport=*/authentication_transport);
  EXPECT_EQ(AuthenticationStatus::kFailure, status);
}

TEST_F(PresenceDeviceProviderTest,
       AuthenticateAsInitiator_NoRemoteSharedCredential) {
  EXPECT_CALL(mock_service_controller_, GetLocalCredentials)
      .WillOnce([&](const CredentialSelector& credential_selector,
                    GetLocalCredentialsResultCallback callback) {
        std::vector<nearby::internal::LocalCredential> credentials;
        credentials.push_back(CreateValidLocalCredential(key_pair_));
        std::move(callback.credentials_fetched_cb)(credentials);
      });

  PresenceDevice remote_device(CreateTestDeviceIdentityMetaData());
  MockAuthenticationTransport authentication_transport;
  auto status = provider_->AuthenticateAsInitiator(
      /*remote_device=*/remote_device, /*shared_secret=*/kUkey2Secret,
      /*authentication_transport=*/authentication_transport);

  EXPECT_EQ(AuthenticationStatus::kFailure, status);
}

TEST_F(PresenceDeviceProviderTest, AuthenticateAsInitiator_FailureToVerify) {
  EXPECT_CALL(mock_service_controller_, GetLocalCredentials)
      .WillOnce([&](const CredentialSelector& credential_selector,
                    GetLocalCredentialsResultCallback callback) {
        std::vector<nearby::internal::LocalCredential> credentials;
        credentials.push_back(CreateValidLocalCredential(key_pair_));
        std::move(callback.credentials_fetched_cb)(credentials);
      });
  EXPECT_CALL(mock_service_controller_, GetLocalPublicCredentials)
      .WillOnce([&](const CredentialSelector& credential_selector,
                    GetPublicCredentialsResultCallback callback) {
        std::vector<nearby::internal::SharedCredential> credentials;
        credentials.push_back(BuildSharedCredential(key_pair_));
        std::move(callback.credentials_fetched_cb)(credentials);
      });

  PresenceDevice remote_device(CreateTestDeviceIdentityMetaData());
  remote_device.SetDecryptSharedCredential(BuildSharedCredential(key_pair_));

  MockAuthenticationTransport authentication_transport;
  EXPECT_CALL(authentication_transport, WriteMessage)
      .WillOnce([&](absl::string_view message) {
        PresenceAuthenticationFrame authentication_frame;
        EXPECT_TRUE(authentication_frame.ParseFromString(message));
        EXPECT_EQ(kPresenceVersion, authentication_frame.version());
      });
  EXPECT_CALL(authentication_transport, ReadMessage).WillOnce([&]() {
    PresenceAuthenticationFrame authentication_frame;
    return authentication_frame.SerializeAsString();
  });

  EXPECT_CALL(mock_connection_authenticator_, BuildSignedMessageAsInitiator)
      .WillOnce(testing::Return(BuildDefaultInitiatorData()));
  EXPECT_CALL(mock_connection_authenticator_, VerifyMessageAsInitiator)
      .WillOnce(testing::Return(
          absl::Status(absl::StatusCode::kCancelled, /*msg=*/std::string())));

  auto status = provider_->AuthenticateAsInitiator(
      /*remote_device=*/remote_device, /*shared_secret=*/kUkey2Secret,
      /*authentication_transport=*/authentication_transport);
  EXPECT_EQ(AuthenticationStatus::kFailure, status);
}

TEST_F(PresenceDeviceProviderTest, AuthenticateAsInitiator_Success) {
  EXPECT_CALL(mock_service_controller_, GetLocalCredentials)
      .WillOnce([&](const CredentialSelector& credential_selector,
                    GetLocalCredentialsResultCallback callback) {
        std::vector<nearby::internal::LocalCredential> credentials;
        credentials.push_back(CreateValidLocalCredential(key_pair_));
        std::move(callback.credentials_fetched_cb)(credentials);
      });
  EXPECT_CALL(mock_service_controller_, GetLocalPublicCredentials)
      .WillOnce([&](const CredentialSelector& credential_selector,
                    GetPublicCredentialsResultCallback callback) {
        std::vector<nearby::internal::SharedCredential> credentials;
        credentials.push_back(BuildSharedCredential(key_pair_));
        std::move(callback.credentials_fetched_cb)(std::move(credentials));
      });

  PresenceDevice remote_device(CreateTestDeviceIdentityMetaData());
  remote_device.SetDecryptSharedCredential(BuildSharedCredential(key_pair_));

  ON_CALL(mock_connection_authenticator_, BuildSignedMessageAsInitiator)
      .WillByDefault(testing::Return(BuildDefaultInitiatorData()));

  MockAuthenticationTransport authentication_transport;
  EXPECT_CALL(authentication_transport, WriteMessage)
      .WillOnce([&](absl::string_view message) {
        PresenceAuthenticationFrame authentication_frame;
        EXPECT_TRUE(authentication_frame.ParseFromString(message));
        EXPECT_EQ(kPresenceVersion, authentication_frame.version());
      });

  auto status = provider_->AuthenticateAsInitiator(
      /*remote_device=*/remote_device, /*shared_secret=*/kUkey2Secret,
      /*authentication_transport=*/authentication_transport);

  EXPECT_EQ(AuthenticationStatus::kSuccess, status);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
