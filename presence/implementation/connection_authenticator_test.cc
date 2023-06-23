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
#include "presence/proto/presence_frame.pb.h"

namespace nearby {
namespace presence {
namespace {

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

TEST(PresenceAuthenticatorTest, TestSignHasV1AuthFrame) {
  ConnectionAuthenticator authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  std::string auth_msg;
  ASSERT_OK_AND_ASSIGN(
      auth_msg,
      authenticator.BuildSignedMessage(
          kUkey2Secret, BuildLocalCredential(key_pair, kKeySeed1), true));
  PresenceFrame frame;
  frame.ParseFromString(auth_msg);
  EXPECT_TRUE(frame.has_v1_frame());
  EXPECT_TRUE(frame.v1_frame().has_authentication_frame());
}

TEST(PresenceAuthenticatorTest, TestInitiatorSignResponderVerify) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  std::string auth_msg;
  ASSERT_OK_AND_ASSIGN(
      auth_msg,
      initiator_authenticator.BuildSignedMessage(
          kUkey2Secret, BuildLocalCredential(key_pair, kKeySeed1), true));
  EXPECT_OK(responder_authenticator.VerifyMessage(
      kUkey2Secret, auth_msg, {BuildSharedCredential(key_pair, kKeySeed1)},
      false));
}

TEST(PresenceAuthenticatorTest, TestResponderSignInitiatorVerify) {
  ConnectionAuthenticator responder_authenticator;
  ConnectionAuthenticator initiator_authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  ASSERT_OK_AND_ASSIGN(
      std::string auth_msg,
      responder_authenticator.BuildSignedMessage(
          kUkey2Secret, BuildLocalCredential(key_pair, kKeySeed1), false));
  EXPECT_OK(initiator_authenticator.VerifyMessage(
      kUkey2Secret, auth_msg, {BuildSharedCredential(key_pair, kKeySeed1)},
      true));
}

TEST(PresenceAuthenticatorTest, TestInitiatorSignInitiatorVerifyFails) {
  ConnectionAuthenticator initiator_authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  std::string auth_msg;
  ASSERT_OK_AND_ASSIGN(
      auth_msg,
      initiator_authenticator.BuildSignedMessage(
          kUkey2Secret, BuildLocalCredential(key_pair, kKeySeed1), true));
  EXPECT_THAT(initiator_authenticator.VerifyMessage(
                  kUkey2Secret, auth_msg,
                  {BuildSharedCredential(key_pair, kKeySeed1)}, true),
              StatusIs(absl::StatusCode::kInternal));
}

TEST(PresenceAuthenticatorTest, TestResponderSignResponderVerifyFails) {
  ConnectionAuthenticator responder_authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  std::string auth_msg;
  ASSERT_OK_AND_ASSIGN(
      auth_msg,
      responder_authenticator.BuildSignedMessage(
          kUkey2Secret, BuildLocalCredential(key_pair, kKeySeed1), false));
  EXPECT_THAT(responder_authenticator.VerifyMessage(
                  kUkey2Secret, auth_msg,
                  {BuildSharedCredential(key_pair, kKeySeed1)}, false),
              StatusIs(absl::StatusCode::kInternal));
}

TEST(PresenceAuthenticatorTest, TestBadKeypairSignVerifyFails) {
  ConnectionAuthenticator authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  auto key_pair_or_status2 = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair2;
  ASSERT_OK_AND_ASSIGN(key_pair2, key_pair_or_status2);
  std::string auth_msg;
  ASSERT_OK_AND_ASSIGN(
      auth_msg,
      authenticator.BuildSignedMessage(
          kUkey2Secret, BuildLocalCredential(key_pair, kKeySeed1), true));
  EXPECT_THAT(authenticator.VerifyMessage(
                  kUkey2Secret, auth_msg,
                  {BuildSharedCredential(key_pair2, kKeySeed1)}, false),
              StatusIs(absl::StatusCode::kInternal));
}

TEST(PresenceAuthenticatorTest, TestBadKeyseedSignVerifyFails) {
  ConnectionAuthenticator authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  std::string auth_msg;
  ASSERT_OK_AND_ASSIGN(
      auth_msg,
      authenticator.BuildSignedMessage(
          kUkey2Secret, BuildLocalCredential(key_pair, kKeySeed1), true));
  EXPECT_THAT(authenticator.VerifyMessage(
                  kUkey2Secret, auth_msg,
                  {BuildSharedCredential(key_pair, kKeySeed2)}, false),
              StatusIs(absl::StatusCode::kInternal));
}

TEST(PresenceAuthenticatorTest, TestBadSharedCredentialSignVerifyFails) {
  ConnectionAuthenticator authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  auto key_pair_or_status2 = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair2;
  ASSERT_OK_AND_ASSIGN(key_pair2, key_pair_or_status2);
  std::string auth_msg;
  ASSERT_OK_AND_ASSIGN(
      auth_msg,
      authenticator.BuildSignedMessage(
          kUkey2Secret, BuildLocalCredential(key_pair, kKeySeed1), true));
  EXPECT_THAT(authenticator.VerifyMessage(
                  kUkey2Secret, auth_msg,
                  {BuildSharedCredential(key_pair2, kKeySeed2)}, false),
              StatusIs(absl::StatusCode::kInternal));
}

TEST(PresenceAuthenticatorTest, TestBadPresenceFrameFails) {
  ConnectionAuthenticator authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  std::string auth_msg = "\x34\x45";
  EXPECT_THAT(authenticator.VerifyMessage(
                  kUkey2Secret, auth_msg,
                  {BuildSharedCredential(key_pair, kKeySeed1)}, false),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(PresenceAuthenticatorTest, TestNoV1FrameFails) {
  PresenceFrame frame;
  ASSERT_FALSE(frame.has_v1_frame());
  ConnectionAuthenticator authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  EXPECT_THAT(authenticator.VerifyMessage(
                  kUkey2Secret, frame.SerializeAsString(),
                  {BuildSharedCredential(key_pair, kKeySeed1)}, false),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(PresenceAuthenticatorTest, TestNoAuthFrameFails) {
  PresenceFrame frame;
  frame.mutable_v1_frame()->clear_authentication_frame();
  ASSERT_TRUE(frame.has_v1_frame());
  ASSERT_FALSE(frame.v1_frame().has_authentication_frame());
  ConnectionAuthenticator authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  EXPECT_THAT(authenticator.VerifyMessage(
                  kUkey2Secret, frame.SerializeAsString(),
                  {BuildSharedCredential(key_pair, kKeySeed1)}, false),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(PresenceAuthenticatorTest, TestBadVersionFails) {
  ConnectionAuthenticator authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  std::string auth_msg;
  ASSERT_OK_AND_ASSIGN(
      auth_msg,
      authenticator.BuildSignedMessage(
          kUkey2Secret, BuildLocalCredential(key_pair, kKeySeed1), true));
  PresenceFrame frame;
  frame.ParseFromString(auth_msg);
  frame.mutable_v1_frame()->mutable_authentication_frame()->set_version(2);
  EXPECT_THAT(authenticator.VerifyMessage(
                  kUkey2Secret, frame.SerializeAsString(),
                  {BuildSharedCredential(key_pair, kKeySeed1)}, false),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(PresenceAuthenticatorTest, TestMissingCredentialIdHashFails) {
  ConnectionAuthenticator authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  std::string auth_msg;
  ASSERT_OK_AND_ASSIGN(
      auth_msg,
      authenticator.BuildSignedMessage(
          kUkey2Secret, BuildLocalCredential(key_pair, kKeySeed1), true));
  PresenceFrame frame;
  frame.ParseFromString(auth_msg);
  frame.mutable_v1_frame()
      ->mutable_authentication_frame()
      ->clear_credential_id_hash();
  EXPECT_THAT(authenticator.VerifyMessage(
                  kUkey2Secret, frame.SerializeAsString(),
                  {BuildSharedCredential(key_pair, kKeySeed1)}, false),
              StatusIs(absl::StatusCode::kInternal));
}

TEST(PresenceAuthenticatorTest, TestMissingPrivateKeySignatureFails) {
  ConnectionAuthenticator authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  std::string auth_msg;
  ASSERT_OK_AND_ASSIGN(
      auth_msg,
      authenticator.BuildSignedMessage(
          kUkey2Secret, BuildLocalCredential(key_pair, kKeySeed1), true));
  PresenceFrame frame;
  frame.ParseFromString(auth_msg);
  frame.mutable_v1_frame()
      ->mutable_authentication_frame()
      ->clear_private_key_signature();
  EXPECT_THAT(authenticator.VerifyMessage(
                  kUkey2Secret, frame.SerializeAsString(),
                  {BuildSharedCredential(key_pair, kKeySeed1)}, false),
              StatusIs(absl::StatusCode::kInternal));
}

TEST(PresenceAuthenticatorTest, TestMissingPublicKeyVerifyFails) {
  ConnectionAuthenticator authenticator;
  auto key_pair_or_status = crypto::Ed25519Signer::CreateNewKeyPair();
  crypto::Ed25519KeyPair key_pair;
  ASSERT_OK_AND_ASSIGN(key_pair, key_pair_or_status);
  std::string auth_msg;
  ASSERT_OK_AND_ASSIGN(
      auth_msg,
      authenticator.BuildSignedMessage(
          kUkey2Secret, BuildLocalCredential(key_pair, kKeySeed1), true));
  key_pair.public_key.clear();
  EXPECT_THAT(authenticator.VerifyMessage(
                  kUkey2Secret, auth_msg,
                  {BuildSharedCredential(key_pair, kKeySeed1)}, true),
              StatusIs(absl::StatusCode::kInternal));
}

}  // namespace
}  // namespace presence
}  // namespace nearby
