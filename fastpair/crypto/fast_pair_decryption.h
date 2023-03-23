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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_FAST_PAIR_DECRYPTION_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_FAST_PAIR_DECRYPTION_H_

#include <array>
#include <string>
#include <optional>

#include "fastpair/crypto/decrypted_passkey.h"
#include "fastpair/crypto/decrypted_response.h"

namespace nearby {
namespace fastpair {
/** Utilities used for encrypting and decrypting Fast Pair packets. */
class FastPairDecryption {
 public:
  static constexpr int kAesBlockByteSize = 16;

  static std::optional<DecryptedResponse> ParseDecryptResponse(
      const std::array<uint8_t, 16>& aes_key_bytes,
      const std::array<uint8_t, 16>& encrypted_response_bytes);

  static std::optional<DecryptedPasskey> ParseDecryptPasskey(
      const std::array<uint8_t, 16>& aes_key_bytes,
      const std::array<uint8_t, 16>& encrypted_passkey_bytes);
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_DECRYPTION_H_
