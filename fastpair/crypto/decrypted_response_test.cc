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

#include "fastpair/crypto/decrypted_response.h"

#include <array>

#include "gtest/gtest.h"

namespace nearby {
namespace fastpair {
namespace {

TEST(DecryptedResponseTest, CreateDecryptedResponse) {
  constexpr std::array<uint8_t, 6> address_bytes = {0x02, 0x03, 0x04,
                                                    0x05, 0x06, 0x07};

  constexpr std::array<uint8_t, 9> salt = {0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                           0x0D, 0x0E, 0x0F, 0x00};

  DecryptedResponse decryptedResponse(
      FastPairMessageType::kKeyBasedPairingResponse, address_bytes, salt);
  EXPECT_EQ(decryptedResponse.message_type,
            FastPairMessageType::kKeyBasedPairingResponse);
  EXPECT_EQ(decryptedResponse.address_bytes, address_bytes);
  EXPECT_EQ(decryptedResponse.salt, salt);
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
