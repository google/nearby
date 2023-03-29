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

#include "fastpair/crypto/fast_pair_encryption.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <string>

#include "absl/strings/escaping.h"
#include "gtest/gtest.h"

namespace nearby {
namespace fastpair {
namespace {

// All test data comes from
// https://developers.google.com/nearby/fast-pair/specifications/appendix/testcases#test_cases

constexpr std::array<uint8_t, kAesBlockByteSize> aes_key_bytes = {
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6,
    0xCF, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};

std::string DecodeKey(const std::string& encoded_key) {
  std::string key;
  absl::Base64Unescape(encoded_key, &key);
  return key;
}

class FastPairEncryptionTest : public testing::Test {};

TEST(FastPairEncryptionTest, EncryptBytes_Success) {
  constexpr std::array<uint8_t, kAesBlockByteSize> input = {
      0xF3, 0x0F, 0x4E, 0x78, 0x6C, 0x59, 0xA7, 0xBB,
      0xF3, 0x87, 0x3B, 0x5A, 0x49, 0xBA, 0x97, 0xEA};

  constexpr std::array<uint8_t, kAesBlockByteSize> expected = {
      0xAC, 0x9A, 0x16, 0xF0, 0x95, 0x3A, 0x3F, 0x22,
      0x3D, 0xD1, 0x0C, 0xF5, 0x36, 0xE0, 0x9E, 0x9C};

  EXPECT_EQ(FastPairEncryption::EncryptBytes(aes_key_bytes, input), expected);
}

TEST(FastPairEncryptionTest, GenerateKeysWithEcdhKeyAgreement_EmptyKey) {
  EXPECT_FALSE(
      FastPairEncryption::GenerateKeysWithEcdhKeyAgreement("").has_value());
}

TEST(FastPairEncryptionTest, GenerateKeysWithEcdhKeyAgreement_ShortKey) {
  EXPECT_FALSE(FastPairEncryption::GenerateKeysWithEcdhKeyAgreement("too_short")
                   .has_value());
}

TEST(FastPairEncryptionTest, GenerateKeysWithEcdhKeyAgreement_InvalidKey) {
  EXPECT_FALSE(
      FastPairEncryption::GenerateKeysWithEcdhKeyAgreement(
          DecodeKey("U2PWc3FHTxah/o0YT9n1VRvtm57SNIRSXOEBXm4fdtMo+06tNoFlt8D0/"
                    "2BsN8auolz5ikwLRvQh+MiQ6oYveg=="))
          .has_value());
}

TEST(FastPairEncryptionTest, GenerateKeysWithEcdhKeyAgreement_ValidKey) {
  EXPECT_TRUE(
      FastPairEncryption::GenerateKeysWithEcdhKeyAgreement(
          DecodeKey("U2PWc3FHTxah/o0YU9n1VRvtm57SNIRSXOEBXm4fdtMo+06tNoFlt8D0/"
                    "2BsN8auolz5ikwLRvQh+MiQ6oYveg=="))
          .has_value());
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
