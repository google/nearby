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

#include "fastpair/crypto/fast_pair_decryption.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <vector>

#include "gtest/gtest.h"
#include "fastpair/crypto/fast_pair_encryption.h"

namespace nearby {
namespace fastpair {
namespace {

// All test data comes from
// https://developers.google.com/nearby/fast-pair/specifications/appendix/testcases#test_cases

constexpr std::array<uint8_t, kAesBlockByteSize> aes_key_bytes = {
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6,
    0xCF, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};

TEST(FastPairDecryptionTest, ParseDecryptResponseSuccess) {
  std::vector<uint8_t> response_bytes;

  // Message type.
  response_bytes.push_back(0x01);

  // Address bytes.
  std::array<uint8_t, 6> address_bytes = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  std::copy(address_bytes.begin(), address_bytes.end(),
            std::back_inserter(response_bytes));

  // Random salt
  std::array<uint8_t, 9> salt = {0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                 0x0D, 0x0E, 0x0F, 0x00};
  std::copy(salt.begin(), salt.end(), std::back_inserter(response_bytes));

  std::array<uint8_t, kAesBlockByteSize> response_bytes_array;
  std::copy_n(response_bytes.begin(), kAesBlockByteSize,
              response_bytes_array.begin());

  auto encrypted_bytes =
      FastPairEncryption::EncryptBytes(aes_key_bytes, response_bytes_array);
  auto response =
      FastPairDecryption::ParseDecryptResponse(aes_key_bytes, encrypted_bytes);

  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->message_type,
            FastPairMessageType::kKeyBasedPairingResponse);
  EXPECT_EQ(response->address_bytes, address_bytes);
  EXPECT_EQ(response->salt, salt);
}

TEST(FastPairDecryptionTest, ParseDecryptResponseFailure) {
  constexpr std::array<uint8_t, kAesBlockByteSize> response_bytes = {
      /*message_type=*/0x02,
      /*address_bytes=*/0x02,
      0x03,
      0x04,
      0x05,
      0x06,
      0x07,
      /*salt=*/0x08,
      0x09,
      0x0A,
      0x0B,
      0x0C,
      0x0D,
      0x0E,
      0x0F,
      0x00};

  auto encrypted_bytes =
      FastPairEncryption::EncryptBytes(aes_key_bytes, response_bytes);
  auto response =
      FastPairDecryption::ParseDecryptResponse(aes_key_bytes, encrypted_bytes);

  EXPECT_FALSE(response.has_value());
}

TEST(FastPairDecryptionTest, ParseDecryptPasskeySuccess) {
  std::vector<uint8_t> passkey_bytes;

  // Message type.
  passkey_bytes.push_back(0x02);

  // Passkey bytes.
  uint32_t passkey = 5;
  passkey_bytes.push_back(passkey >> 16);
  passkey_bytes.push_back(passkey >> 8);
  passkey_bytes.push_back(passkey);

  // Random salt
  std::array<uint8_t, 12> salt = {0x08, 0x09, 0x0A, 0x08, 0x09, 0x0E,
                                  0x0A, 0x0C, 0x0D, 0x0E, 0x05, 0x02};
  std::copy(salt.begin(), salt.end(), std::back_inserter(passkey_bytes));

  std::array<uint8_t, kAesBlockByteSize> passkey_bytes_array;
  std::copy_n(passkey_bytes.begin(), kAesBlockByteSize,
              passkey_bytes_array.begin());

  auto encrypted_bytes =
      FastPairEncryption::EncryptBytes(aes_key_bytes, passkey_bytes_array);
  auto decrypted_passkey =
      FastPairDecryption::ParseDecryptPasskey(aes_key_bytes, encrypted_bytes);

  EXPECT_TRUE(decrypted_passkey.has_value());
  EXPECT_EQ(decrypted_passkey->message_type,
            FastPairMessageType::kSeekersPasskey);
  EXPECT_EQ(decrypted_passkey->passkey, passkey);
  EXPECT_EQ(decrypted_passkey->salt, salt);
}

TEST(FastPairDecryptionTest, ParseDecryptPasskeyFailure) {
  constexpr std::array<uint8_t, kAesBlockByteSize> passkey_bytes = {
      /*message_type=*/0x04,
      /*passkey=*/0x02,
      0x03,
      0x04,
      /*salt=*/0x05,
      0x06,
      0x07,
      0x08,
      0x09,
      0x0A,
      0x0B,
      0x0C,
      0x0D,
      0x0E,
      0x0F,
      0x0E};

  auto encrypted_bytes =
      FastPairEncryption::EncryptBytes(aes_key_bytes, passkey_bytes);
  auto passkey =
      FastPairDecryption::ParseDecryptPasskey(aes_key_bytes, encrypted_bytes);

  EXPECT_FALSE(passkey.has_value());
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
