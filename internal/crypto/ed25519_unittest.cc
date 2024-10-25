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

#include "internal/crypto/ed25519.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/escaping.h"

namespace nearby::crypto {
namespace {

using ::absl::StatusCode;
using ::testing::status::StatusIs;

// From
// https://github.com/google/boringssl/blob/master/crypto/curve25519/ed25519_tests.txt
TEST(Ed25519SignerTest, SignatureIsCorrect1) {
  std::string priv = absl::HexStringToBytes(
      "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60d75a9801"
      "82b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
  ASSERT_EQ(priv.size(), 64);
  auto signer = Ed25519Signer::Create(priv);
  ASSERT_OK(signer);
  auto signature = signer->Sign("");
  ASSERT_TRUE(signature.has_value());
  EXPECT_EQ(
      *signature,
      absl::HexStringToBytes(
          "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8"
          "821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"));
}

TEST(Ed25519SignerTest, SignatureIsCorrect2) {
  std::string priv = absl::HexStringToBytes(
      "4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb3d4017c3"
      "e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c");
  ASSERT_EQ(priv.size(), 64);
  auto signer = Ed25519Signer::Create(priv);
  ASSERT_OK(signer);
  auto signature = signer->Sign(absl::HexStringToBytes("72"));
  ASSERT_TRUE(signature.has_value());
  std::string expected_sig = absl::HexStringToBytes(
      "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085a"
      "c1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00");
  ASSERT_EQ(expected_sig.size(), 64);
  EXPECT_EQ(*signature, expected_sig);
}

TEST(Ed25519SignerTest, SignatureIsCorrect3) {
  std::string priv = absl::HexStringToBytes(
      "c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7fc51cd8e"
      "6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025");
  ASSERT_EQ(priv.size(), 64);
  auto signer = Ed25519Signer::Create(priv);
  ASSERT_OK(signer);
  auto signature = signer->Sign(absl::HexStringToBytes("af82"));
  ASSERT_TRUE(signature.has_value());
  EXPECT_EQ(
      *signature,
      absl::HexStringToBytes(
          "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff"
          "9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a"));
}

TEST(Ed25519SignerTest, RejectsBadKeyLength) {
  std::string priv = absl::HexStringToBytes(
      "c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7fc51cd8e"
      "6218a1a38da47ed00230f0580816ed13ba3303ac5deb91154890802570");
  ASSERT_EQ(priv.size(), 65);
  auto signer = Ed25519Signer::Create(priv);
  EXPECT_THAT(signer, StatusIs(StatusCode::kInvalidArgument));
}

// Wycheproof vectors
TEST(Ed25519SignerTest, NewEd25519KeyValidSeed1) {
  std::string private_key = absl::HexStringToBytes(
      "add4bb8103785baf9ac534258e8aaf65f5f1adb5ef5f3df19bb80ab989c4d64b");
  std::string public_key = absl::HexStringToBytes(
      "7d4d0e7f6153a69b6242b522abbee685fda4420f8834b108c3bdae369ef549fa");
  auto key = Ed25519Signer::CreateNewKeyPair(private_key);
  ASSERT_OK(key);
  EXPECT_EQ(key->public_key, public_key);
  EXPECT_EQ(key->private_key, private_key);
}

TEST(Ed25519SignerTest, NewEd25519KeyValidSeed2) {
  std::string private_key = absl::HexStringToBytes(
      "0a23a20072891237aa0864b5765139514908787878cd77135a0059881d313f00");
  std::string public_key = absl::HexStringToBytes(
      "a12c2beb77265f2aac953b5009349d94155a03ada416aad451319480e983ca4c");
  auto key = Ed25519Signer::CreateNewKeyPair(private_key);
  ASSERT_OK(key);
  EXPECT_EQ(key->public_key, public_key);
  EXPECT_EQ(key->private_key, private_key);
}

TEST(Ed25519SignerTest, NewEd25519KeyValidSeed3) {
  std::string private_key = absl::HexStringToBytes(
      "4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb");
  std::string public_key = absl::HexStringToBytes(
      "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c");
  auto key = Ed25519Signer::CreateNewKeyPair(private_key);
  ASSERT_OK(key);
  EXPECT_EQ(key->public_key, public_key);
  EXPECT_EQ(key->private_key, private_key);
}

TEST(Ed25519SignerTest, NewEd25519KeyInvalidSeed) {
  std::string valid_seed = absl::HexStringToBytes(
      "000102030405060708090a0b0c0d0e0f000102030405060708090a0b0c0d0e0f");
  // Seed that is too small.
  for (int i = 0; i < 32; i++) {
    EXPECT_THAT(Ed25519Signer::CreateNewKeyPair(valid_seed.substr(0, i)),
                StatusIs(StatusCode::kInvalidArgument));
  }
  // Seed that is too large.
  std::string large_seed = absl::StrCat(valid_seed, "a");
  EXPECT_THAT(Ed25519Signer::CreateNewKeyPair(large_seed),
              StatusIs(StatusCode::kInvalidArgument));
}

TEST(Ed25519VerifierTest, CanVerifyCorrect1) {
  std::string pub = absl::HexStringToBytes(
      "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
  ASSERT_EQ(pub.size(), 32);
  auto verifier = Ed25519Verifier::Create(pub);
  ASSERT_OK(verifier);
  EXPECT_OK(verifier->Verify(
      "",
      absl::HexStringToBytes(
          "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8"
          "821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b")));
}

TEST(Ed25519VerifierTest, CanVerifyCorrect2) {
  std::string pub = absl::HexStringToBytes(
      "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c");
  ASSERT_EQ(pub.size(), 32);
  auto verifier = Ed25519Verifier::Create(pub);
  ASSERT_OK(verifier);
  EXPECT_OK(verifier->Verify(
      absl::HexStringToBytes("72"),
      absl::HexStringToBytes(
          "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085a"
          "c1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00")));
}

TEST(Ed25519VerifierTest, CanVerifyCorrect3) {
  std::string pub = absl::HexStringToBytes(
      "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025");
  ASSERT_EQ(pub.size(), 32);
  auto verifier = Ed25519Verifier::Create(pub);
  ASSERT_OK(verifier);
  EXPECT_OK(verifier->Verify(
      absl::HexStringToBytes("af82"),
      absl::HexStringToBytes(
          "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff"
          "9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a")));
}

TEST(Ed25519VerifierTest, DoesNotVerifyBadSignature) {
  std::string pub = absl::HexStringToBytes(
      "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025");
  ASSERT_EQ(pub.size(), 32);
  auto verifier = Ed25519Verifier::Create(pub);
  ASSERT_OK(verifier);
  EXPECT_THAT(verifier->Verify(
                  absl::HexStringToBytes("af82"),
                  // Added 0x80 to signature.
                  absl::HexStringToBytes("6291d657deec24024827e69c3abe01a30ce54"
                                         "8a284743a445e3680d7db5ac3ac18ff"
                                         "9b538d16f290ae67f760984dc6594a7c15e97"
                                         "16ed28dc027beceea1ec40a80")),
              StatusIs(StatusCode::kInvalidArgument));
}

TEST(Ed25519VerifierTest, RejectsBadKeyLength) {
  std::string pub = absl::HexStringToBytes(
      "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb91154890802570");
  ASSERT_EQ(pub.size(), 33);
  auto verifier = Ed25519Verifier::Create(pub);
  EXPECT_THAT(verifier, StatusIs(StatusCode::kInvalidArgument));
}

TEST(Ed25519SignerVerifierTest, NewKeypairFromRandomSeedRoundtrip) {
  auto key_pair = Ed25519Signer::CreateNewKeyPair();
  ASSERT_NE(key_pair->private_key, std::string(32, 0));
  auto signer = Ed25519Signer::Create(
      absl::StrCat(key_pair->private_key, key_pair->public_key));
  ASSERT_OK(signer);
  auto verifier = Ed25519Verifier::Create(key_pair->public_key);
  ASSERT_OK(verifier);
  auto signature = signer->Sign("hello world");
  ASSERT_TRUE(signature.has_value());
  EXPECT_OK(verifier->Verify("hello world", *signature));
}

}  // namespace
}  // namespace nearby::crypto
