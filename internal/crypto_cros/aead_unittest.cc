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

#include "internal/crypto_cros/aead.h"

#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace nearby {
namespace {

const crypto::Aead::AeadAlgorithm kAllAlgorithms[]{
    crypto::Aead::AES_128_CTR_HMAC_SHA256,
    crypto::Aead::AES_256_GCM,
    crypto::Aead::AES_256_GCM_SIV,
    crypto::Aead::CHACHA20_POLY1305,
};

class AeadTest : public testing::TestWithParam<crypto::Aead::AeadAlgorithm> {};

INSTANTIATE_TEST_SUITE_P(All, AeadTest, testing::ValuesIn(kAllAlgorithms));

TEST_P(AeadTest, SealOpen) {
  crypto::Aead::AeadAlgorithm alg = GetParam();
  crypto::Aead aead(alg);
  std::string key(aead.KeyLength(), 0);
  aead.Init(&key);
  std::string nonce(aead.NonceLength(), 0);
  std::string plaintext("this is the plaintext");
  std::string ad("this is the additional data");
  std::string ciphertext;
  EXPECT_TRUE(aead.Seal(plaintext, nonce, ad, &ciphertext));
  EXPECT_LT(0U, ciphertext.size());

  std::string decrypted;
  EXPECT_TRUE(aead.Open(ciphertext, nonce, ad, &decrypted));

  EXPECT_EQ(plaintext, decrypted);
}

TEST_P(AeadTest, SealOpenSpan) {
  crypto::Aead::AeadAlgorithm alg = GetParam();
  crypto::Aead aead(alg);
  std::vector<uint8_t> key(aead.KeyLength(), 0u);
  aead.Init(key);
  std::vector<uint8_t> nonce(aead.NonceLength(), 0u);
  static constexpr uint8_t kPlaintext[] = "plaintext";
  static constexpr uint8_t kAdditionalData[] = "additional data input";
  std::vector<uint8_t> ciphertext =
      aead.Seal(kPlaintext, nonce, kAdditionalData);
  EXPECT_LT(sizeof(kPlaintext), ciphertext.size());

  absl::optional<std::vector<uint8_t>> decrypted =
      aead.Open(ciphertext, nonce, kAdditionalData);
  ASSERT_TRUE(decrypted);
  ASSERT_EQ(decrypted->size(), sizeof(kPlaintext));
  ASSERT_EQ(0, memcmp(decrypted->data(), kPlaintext, sizeof(kPlaintext)));

  std::vector<uint8_t> wrong_key(aead.KeyLength(), 1u);
  crypto::Aead aead_wrong_key(alg);
  aead_wrong_key.Init(wrong_key);
  decrypted = aead_wrong_key.Open(ciphertext, nonce, kAdditionalData);
  EXPECT_FALSE(decrypted);
}

TEST_P(AeadTest, SealOpenWrongKey) {
  crypto::Aead::AeadAlgorithm alg = GetParam();
  crypto::Aead aead(alg);
  std::string key(aead.KeyLength(), 0);
  std::string wrong_key(aead.KeyLength(), 1);
  aead.Init(&key);
  crypto::Aead aead_wrong_key(alg);
  aead_wrong_key.Init(&wrong_key);

  std::string nonce(aead.NonceLength(), 0);
  std::string plaintext("this is the plaintext");
  std::string ad("this is the additional data");
  std::string ciphertext;
  EXPECT_TRUE(aead.Seal(plaintext, nonce, ad, &ciphertext));
  EXPECT_LT(0U, ciphertext.size());

  std::string decrypted;
  EXPECT_FALSE(aead_wrong_key.Open(ciphertext, nonce, ad, &decrypted));
  EXPECT_EQ(0U, decrypted.size());
}

}  // namespace
}  // namespace nearby
