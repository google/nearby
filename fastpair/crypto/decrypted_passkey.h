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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_DECRYPTED_PASSKEY_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_DECRYPTED_PASSKEY_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "fastpair/common/constant.h"
#include "fastpair/crypto/fast_pair_message_type.h"

namespace nearby {
namespace fastpair {

// Thin structure which is used by the higher level components of the Fast Pair
// system to represent a decrypted account passkey.
struct DecryptedPasskey {
  DecryptedPasskey(
      FastPairMessageType message_type, uint32_t passkey,
      const std::array<uint8_t, kDecryptedPasskeySaltByteSize>& salt)
      : message_type(message_type), passkey(passkey), salt(salt) {}

  FastPairMessageType message_type;
  uint32_t passkey;
  std::array<uint8_t, kDecryptedPasskeySaltByteSize> salt;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_DECRYPTED_PASSKEY_H_
