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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "fastpair/common/constant.h"
#include "fastpair/crypto/decrypted_passkey.h"
#include "fastpair/crypto/decrypted_response.h"

namespace nearby {
namespace fastpair {

// Holds a secret key for a device and has methods to encrypt bytes, decrypt
// response and decrypt passkey.
class FastPairDataEncryptor {
 public:
  virtual ~FastPairDataEncryptor() = default;

  // Encrypts bytes with the stored secret key.
  virtual std::array<uint8_t, kAesBlockByteSize> EncryptBytes(
      const std::array<uint8_t, kAesBlockByteSize>& bytes_to_encrypt) const = 0;

  // Return stored PublicKey
  virtual std::optional<std::array<uint8_t, 64>> GetPublicKey() const = 0;

  // Decrypt and parse decrypted response bytes with the stored secret key.
  virtual void ParseDecryptResponse(
      const std::vector<uint8_t>& encrypted_response_bytes,
      absl::AnyInvocable<void(const std::optional<DecryptedResponse>)>
          callback) = 0;

  // Decrypt and parse decrypted passkey bytes with the stored secret key.
  virtual void ParseDecryptPasskey(
      const std::vector<uint8_t>& encrypted_passkey_bytes,
      absl::AnyInvocable<void(const std::optional<DecryptedPasskey>)>
          callback) = 0;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_H_
