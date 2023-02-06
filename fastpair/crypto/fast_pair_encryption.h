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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_FAST_PAIR_ENCRYPTION_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_FAST_PAIR_ENCRYPTION_H_

#include <stdint.h>

#include <array>
#include <string>
#include <string_view>
#include <optional>

#include "fastpair/common/constant.h"
#include "fastpair/crypto/fast_pair_key_pair.h"

namespace nearby {
namespace fastpair {

/** Utilities used for generating Ecdh key agreement and
 * encrypting Fast Pair packets. */
class FastPairEncryption {
 public:
  static std::optional<KeyPair> GenerateKeysWithEcdhKeyAgreement(
      std::string_view decoded_public_anti_spoofing);

  static std::array<uint8_t, kAesBlockByteSize> EncryptBytes(
      const std::array<uint8_t, kAesBlockByteSize>& aes_key_bytes,
      const std::array<uint8_t, kAesBlockByteSize>& bytes_to_encrypt);
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_FAST_PAIR_ENCRYPTION_H_
