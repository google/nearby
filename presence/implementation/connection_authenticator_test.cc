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

#include "presence/implementation/connection_authenticator.h"

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/crypto/ed25519.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/local_credential.pb.h"

namespace nearby {
namespace presence {
namespace {

using ::protobuf_matchers::EqualsProto;
using ::testing::status::StatusIs;

constexpr char kUkey2Secret[] = {0x34, 0x56, 0x78, 0x90};
constexpr char kKeySeed1[] = {1, 2, 3, 4, 5, 6, 7, 8};
constexpr char kKeySeed2[] = {8, 7, 6, 5, 4, 3, 2, 1};

internal::LocalCredential BuildLocalCredential(
    const crypto::Ed25519KeyPair& key_pair, absl::string_view key_seed) {
  internal::LocalCredential local_credential;
  local_credential.mutable_connection_signing_key()->set_key(
      absl::StrCat(key_pair.private_key, key_pair.public_key));
  local_credential.set_key_seed(key_seed);
  return local_credential;
}

internal::SharedCredential BuildSharedCredential(
    const crypto::Ed25519KeyPair& key_pair, absl::string_view key_seed) {
  internal::SharedCredential shared_credential;
  shared_credential.set_connection_signature_verification_key(
      key_pair.public_key);
  shared_credential.set_key_seed(key_seed);
  return shared_credential;
}

class PresenceAuthenticatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
    ASSERT_OK_AND_ASSIGN(auto key_pair1, key_pair_or_status);
    auto key_pair2_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
    ASSERT_OK_AND_ASSIGN(auto key_pair2, key_pair2_or_status);
    initiator_local_credential_ = BuildLocalCredential(key_pair1, kKeySeed1);
    initiator_shared_credential_ = BuildSharedCredential(key_pair1, kKeySeed1);
    initiator_shared_credential_wrong_key_ =
        BuildSharedCredential(key_pair2, kKeySeed1);
    responder_local_credential_ = BuildLocalCredential(key_pair2, kKeySeed2);
    responder_shared_credential_ = BuildSharedCredential(key_pair2, kKeySeed2);
    responder_shared_credential_wrong_key_ =
        BuildSharedCredential(key_pair1, kKeySeed2);
  }

  internal::LocalCredential initiator_local_credential_;
  internal::LocalCredential responder_local_credential_;
  internal::SharedCredential initiator_shared_credential_;
  internal::SharedCredential initiator_shared_credential_wrong_key_;
  internal::SharedCredential responder_shared_credential_;
  internal::SharedCredential responder_shared_credential_wrong_key_;
};

TEST_F(PresenceAuthenticatorTest, TestTwoWayInitiatorSignResponderVerify) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(ConnectionAuthenticator::InitiatorData auth_data,
                       initiator_authenticator.BuildSignedMessageAsInitiator(
                           kUkey2Secret, initiator_local_credential_,
                           responder_shared_credential_));
  auto local_credential = responder_authenticator.VerifyMessageAsResponder(
      kUkey2Secret, auth_data, {responder_local_credential_},
      {initiator_shared_credential_});
  ASSERT_TRUE(local_credential.ok());
  EXPECT_THAT(*local_credential, EqualsProto(responder_local_credential_));
}

TEST_F(PresenceAuthenticatorTest, TestOneWayInitiatorSignResponderVerify) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(
      ConnectionAuthenticator::InitiatorData auth_data,
      initiator_authenticator.BuildSignedMessageAsInitiator(
          kUkey2Secret, std::nullopt, responder_shared_credential_));
  auto local_credential = responder_authenticator.VerifyMessageAsResponder(
      kUkey2Secret, auth_data, {responder_local_credential_},
      {initiator_shared_credential_});
  ASSERT_TRUE(local_credential.ok());
  EXPECT_THAT(*local_credential, EqualsProto(responder_local_credential_));
}

TEST_F(PresenceAuthenticatorTest, TestResponderSignInitiatorVerify) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(ConnectionAuthenticator::ResponderData auth_data,
                       responder_authenticator.BuildSignedMessageAsResponder(
                           kUkey2Secret, responder_local_credential_));
  EXPECT_OK(initiator_authenticator.VerifyMessageAsInitiator(
      auth_data, kUkey2Secret, {responder_shared_credential_}));
}

TEST_F(PresenceAuthenticatorTest,
       TestTwoWayInitiatorSignResponderVerifyNoSharedCredentialMatchFails) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(ConnectionAuthenticator::InitiatorData auth_data,
                       initiator_authenticator.BuildSignedMessageAsInitiator(
                           kUkey2Secret, initiator_local_credential_,
                           responder_shared_credential_));
  EXPECT_THAT(responder_authenticator.VerifyMessageAsResponder(
                  kUkey2Secret, auth_data, {}, {initiator_shared_credential_}),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(PresenceAuthenticatorTest,
       TestOneWayInitiatorSignResponderVerifyNoMatchCredentialFails) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(
      ConnectionAuthenticator::InitiatorData auth_data,
      initiator_authenticator.BuildSignedMessageAsInitiator(
          kUkey2Secret, std::nullopt, responder_shared_credential_));
  EXPECT_THAT(responder_authenticator.VerifyMessageAsResponder(
                  kUkey2Secret, auth_data, {}, {initiator_shared_credential_}),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(PresenceAuthenticatorTest,
       TestOneWayInitiatorSignResponderVerifyNoCredentialFails) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(
      ConnectionAuthenticator::InitiatorData auth_data,
      initiator_authenticator.BuildSignedMessageAsInitiator(
          kUkey2Secret, std::nullopt, responder_shared_credential_));
  EXPECT_THAT(responder_authenticator.VerifyMessageAsResponder(
                  kUkey2Secret, auth_data, {}, {}),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(PresenceAuthenticatorTest,
       TestTwoWayInitiatorSignResponderVerifyNoMatchCredentialFails) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(ConnectionAuthenticator::InitiatorData auth_data,
                       initiator_authenticator.BuildSignedMessageAsInitiator(
                           kUkey2Secret, initiator_local_credential_,
                           responder_shared_credential_));
  EXPECT_THAT(responder_authenticator.VerifyMessageAsResponder(
                  kUkey2Secret, auth_data, {}, {initiator_shared_credential_}),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(PresenceAuthenticatorTest,
       TestTwoWayInitiatorSignResponderVerifyWrongKeyFails) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(ConnectionAuthenticator::InitiatorData auth_data,
                       initiator_authenticator.BuildSignedMessageAsInitiator(
                           kUkey2Secret, initiator_local_credential_,
                           responder_shared_credential_));
  EXPECT_THAT(responder_authenticator.VerifyMessageAsResponder(
                  kUkey2Secret, auth_data, {responder_local_credential_},
                  {initiator_shared_credential_wrong_key_}),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(PresenceAuthenticatorTest,
       TestResponderSignInitiatorVerifyNoMatchCredentialFails) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(ConnectionAuthenticator::ResponderData auth_data,
                       responder_authenticator.BuildSignedMessageAsResponder(
                           kUkey2Secret, responder_local_credential_));
  EXPECT_THAT(initiator_authenticator.VerifyMessageAsInitiator(
                  auth_data, kUkey2Secret, {}),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(PresenceAuthenticatorTest,
       TestResponderSignInitiatorVerifyWrongKeyFails) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(ConnectionAuthenticator::ResponderData auth_data,
                       responder_authenticator.BuildSignedMessageAsResponder(
                           kUkey2Secret, responder_local_credential_));
  EXPECT_THAT(
      initiator_authenticator.VerifyMessageAsInitiator(
          auth_data, kUkey2Secret, {responder_shared_credential_wrong_key_}),
      StatusIs(absl::StatusCode::kInternal));
}

TEST_F(PresenceAuthenticatorTest,
       TestTwoWayInitiatorSignResponderVerifyNoCidHashFails) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(ConnectionAuthenticator::InitiatorData auth_data,
                       initiator_authenticator.BuildSignedMessageAsInitiator(
                           kUkey2Secret, initiator_local_credential_,
                           responder_shared_credential_));
  std::get<ConnectionAuthenticator::TwoWayInitiatorData>(auth_data)
      .shared_credential_hash.clear();
  EXPECT_THAT(responder_authenticator.VerifyMessageAsResponder(
                  kUkey2Secret, auth_data, {responder_local_credential_},
                  {initiator_shared_credential_wrong_key_}),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(PresenceAuthenticatorTest,
       TestTwoWayInitiatorSignResponderVerifyNoPkeySigFails) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(ConnectionAuthenticator::InitiatorData auth_data,
                       initiator_authenticator.BuildSignedMessageAsInitiator(
                           kUkey2Secret, initiator_local_credential_,
                           responder_shared_credential_));
  std::get<ConnectionAuthenticator::TwoWayInitiatorData>(auth_data)
      .private_key_signature.clear();
  EXPECT_THAT(responder_authenticator.VerifyMessageAsResponder(
                  kUkey2Secret, auth_data, {responder_local_credential_},
                  {initiator_shared_credential_wrong_key_}),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(PresenceAuthenticatorTest,
       TestOneWayInitiatorSignResponderVerifyNoCidHashFails) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(
      ConnectionAuthenticator::InitiatorData auth_data,
      initiator_authenticator.BuildSignedMessageAsInitiator(
          kUkey2Secret, std::nullopt, responder_shared_credential_));
  std::get<ConnectionAuthenticator::OneWayInitiatorData>(auth_data)
      .shared_credential_hash.clear();
  EXPECT_THAT(responder_authenticator.VerifyMessageAsResponder(
                  kUkey2Secret, auth_data, {responder_local_credential_},
                  {initiator_shared_credential_}),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(PresenceAuthenticatorTest,
       TestResponderSignInitiatorVerifyNoPkeySigFail) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  ASSERT_OK_AND_ASSIGN(ConnectionAuthenticator::ResponderData auth_data,
                       responder_authenticator.BuildSignedMessageAsResponder(
                           kUkey2Secret, responder_local_credential_));
  auth_data.private_key_signature.clear();
  EXPECT_THAT(initiator_authenticator.VerifyMessageAsInitiator(
                  auth_data, kUkey2Secret, {responder_shared_credential_}),
              StatusIs(absl::StatusCode::kInvalidArgument));
}
}  // namespace
}  // namespace presence
}  // namespace nearby
