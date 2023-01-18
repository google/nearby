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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_DECRYPTED_RESPONSE_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_DECRYPTED_RESPONSE_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "fastpair/common/constant.h"
#include "fastpair/crypto/fast_pair_message_type.h"

namespace nearby {
namespace fastpair {

// Thin structure which is used by the higher level components of the Fast Pair
// system to represent a decrypted response.
struct DecryptedResponse {
  DecryptedResponse(
      FastPairMessageType message_type,
      const std::array<uint8_t, kDecryptedResponseAddressByteSize>&
          address_bytes,
      const std::array<uint8_t, kDecryptedResponseSaltByteSize>& salt)
      : message_type(message_type), address_bytes(address_bytes), salt(salt) {}

  FastPairMessageType message_type;
  std::array<uint8_t, kDecryptedResponseAddressByteSize> address_bytes;
  std::array<uint8_t, kDecryptedResponseSaltByteSize> salt;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_DECRYPTED_RESPONSE_H_
