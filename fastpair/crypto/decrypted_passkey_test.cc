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

#include "fastpair/crypto/decrypted_passkey.h"

#include <array>

#include "gtest/gtest.h"

namespace nearby {
namespace fastpair {
namespace {

TEST(DecryptedPasskeyTest, CreateDecryptedPasskey) {
  // Message type
  FastPairMessageType messgaeType = FastPairMessageType::kSeekersPasskey;
  // Passkey bytes.
  std::array<uint8_t, 3> passkey_bytes = {0x02, 0x03, 0x04};
  uint32_t passkey = passkey_bytes[2];
  passkey += passkey_bytes[1] << 8;
  passkey += passkey_bytes[0] << 16;

  // Random salt
  std::array<uint8_t, 12> salt = {0x08, 0x09, 0x0A, 0x08, 0x09, 0x0E,
                                  0x0A, 0x0C, 0x0D, 0x0E, 0x05, 0x02};

  DecryptedPasskey decryptedPasskey(messgaeType, passkey, salt);
  EXPECT_EQ(decryptedPasskey.message_type, messgaeType);
  EXPECT_EQ(decryptedPasskey.passkey, passkey);
  EXPECT_EQ(decryptedPasskey.salt, salt);
}
}  // namespace
}  // namespace fastpair
}  // namespace nearby
