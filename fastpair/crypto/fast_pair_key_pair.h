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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_FAST_PAIR_KEY_PAIR_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_FAST_PAIR_KEY_PAIR_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <utility>

#include "fastpair/common/constant.h"

namespace nearby {
namespace fastpair {

// Key pair structure to represent public and private keys used for encryption/
// decryption.
struct KeyPair {
  KeyPair(
      const std::array<uint8_t, kSharedSecretKeyByteSize>& shared_secret_key,
      const std::array<uint8_t, kPublicKeyByteSize>& public_key)
      : shared_secret_key(std::move(shared_secret_key)),
        public_key(std::move(public_key)) {}

  const std::array<uint8_t, kSharedSecretKeyByteSize> shared_secret_key;
  const std::array<uint8_t, kPublicKeyByteSize> public_key;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_FAST_PAIR_KEY_PAIR_H_
