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

#include "fastpair/crypto/fast_pair_key_pair.h"

#include <array>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace fastpair {
namespace {

// Test data comes from
// https://developers.google.com/nearby/fast-pair/specifications/appendix/testcases#test_cases
TEST(KeyPairTest, CreateKeyPair) {
  // Shared secret key bytes.
  constexpr std::array<uint8_t, kSharedSecretKeyByteSize>
      shared_secret_key_bytes = {0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F,
                                 0xF7, 0xB6, 0xCF, 0x5E, 0x3F, 0x45,
                                 0x61, 0xC3, 0x32, 0x1D};

  // Public key bytes.
  constexpr std::array<uint8_t, kPublicKeyByteSize> public_key_bytes = {
      0x04, 0xb9, 0xda, 0x0d, 0x71, 0x60, 0xb3, 0x63, 0x28, 0x22, 0x67,
      0xe7, 0xe0, 0xa3, 0xf8, 0x00, 0x8e, 0x4c, 0x89, 0xed, 0x31, 0x34,
      0xf6, 0xdb, 0xc4, 0xfe, 0x0b, 0x5d, 0xe1, 0x11, 0x39, 0x49, 0xa6,
      0x50, 0xa8, 0xe3, 0x4a, 0xc0, 0x40, 0x88, 0xb8, 0x38, 0x3f, 0x56,
      0xfb, 0x33, 0x8d, 0xd4, 0x64, 0x91, 0xd6, 0x15, 0x77, 0x42, 0x27,
      0xc5, 0xaa, 0x44, 0xff, 0xab, 0x4d, 0xb5, 0x7e, 0x25};

  KeyPair keyPair(shared_secret_key_bytes, public_key_bytes);

  EXPECT_EQ(keyPair.shared_secret_key, shared_secret_key_bytes);
  EXPECT_EQ(keyPair.public_key, public_key_bytes);
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
