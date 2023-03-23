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

#include "fastpair/crypto/fast_pair_decryption.h"

#include <algorithm>
#include <array>
#include <optional>

#ifdef NEARBY_CHROMIUM
#include "base/check.h"
#elif defined(NEARBY_SWIFTPM)
#include "internal/platform/logging.h"
#else
#include "absl/log/check.h"  // nogncheck
#endif

#include "fastpair/common/constant.h"
#include "fastpair/crypto/decrypted_passkey.h"
#include "fastpair/crypto/decrypted_response.h"
#include <openssl/aes.h>

namespace nearby {
namespace fastpair {
namespace {
std::array<uint8_t, kAesBlockByteSize> DecryptBytes(
    const std::array<uint8_t, kAesBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kAesBlockByteSize>& encrypted_bytes) {
  AES_KEY aes_key;
  int aes_key_was_set = AES_set_decrypt_key(aes_key_bytes.data(),
                                            aes_key_bytes.size() * 8, &aes_key);
  CHECK(aes_key_was_set == 0) << "Invalid AES key size.";
  std::array<uint8_t, kAesBlockByteSize> decrypted_bytes;
  // Encrypted_bytes is less than 16 bytes and can be guaranteed to be a
  // single block, so we encrypt/decrypt it using AES ECB mode (Approved by:
  // b/73360609)
  AES_decrypt(encrypted_bytes.data(), decrypted_bytes.data(), &aes_key);
  return decrypted_bytes;
}
}  // namespace

// Decrypts the encrypted response
// (https://developers.google.com/nearby/fast-pair/specifications/characteristics#table1.4)
// and returns the parsed decrypted response
// (https://developers.google.com/nearby/fast-pair/specifications/characteristics#table1.3)
std::optional<DecryptedResponse> FastPairDecryption::ParseDecryptResponse(
    const std::array<uint8_t, kAesBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kAesBlockByteSize>& encrypted_response_bytes) {
  std::array<uint8_t, kAesBlockByteSize> decrypted_response_bytes =
      DecryptBytes(aes_key_bytes, encrypted_response_bytes);

  uint8_t message_type = decrypted_response_bytes[kMessageTypeIndex];

  // If the message type index is not the expected fast pair message type, then
  // this is not a valid fast pair response.
  if (message_type != kKeybasedPairingResponseType) {
    return std::nullopt;
  }

  std::array<uint8_t, kDecryptedResponseAddressByteSize> address_bytes;
  std::copy(decrypted_response_bytes.begin() + kResponseAddressStartIndex,
            decrypted_response_bytes.begin() + kResponseSaltStartIndex,
            address_bytes.begin());

  std::array<uint8_t, kDecryptedResponseSaltByteSize> salt;
  std::copy(decrypted_response_bytes.begin() + kResponseSaltStartIndex,
            decrypted_response_bytes.end(), salt.begin());
  return DecryptedResponse(FastPairMessageType::kKeyBasedPairingResponse,
                           address_bytes, salt);
}

// Decrypts the encrypted passkey
// (https://developers.google.com/nearby/fast-pair/specifications/characteristics#table2.1)
// and returns the parsed decrypted passkey
// (https://developers.google.com/nearby/fast-pair/specifications/characteristics#table2.2)
// TODO(b/263400788) Add unit test to cover this function and fix all Mutants
// warning
std::optional<DecryptedPasskey> FastPairDecryption::ParseDecryptPasskey(
    const std::array<uint8_t, kAesBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kAesBlockByteSize>& encrypted_passkey_bytes) {
  std::array<uint8_t, kAesBlockByteSize> decrypted_passkey_bytes =
      DecryptBytes(aes_key_bytes, encrypted_passkey_bytes);

  FastPairMessageType message_type;
  if (decrypted_passkey_bytes[kMessageTypeIndex] == kSeekerPasskeyType) {
    message_type = FastPairMessageType::kSeekersPasskey;
  } else if (decrypted_passkey_bytes[kMessageTypeIndex] ==
             kProviderPasskeyType) {
    message_type = FastPairMessageType::kProvidersPasskey;
  } else {
    return std::nullopt;
  }

  uint32_t passkey = decrypted_passkey_bytes[3];
  passkey += decrypted_passkey_bytes[2] << 8;
  passkey += decrypted_passkey_bytes[1] << 16;

  std::array<uint8_t, kDecryptedPasskeySaltByteSize> salt;
  std::copy(decrypted_passkey_bytes.begin() + kPasskeySaltStartIndex,
            decrypted_passkey_bytes.end(), salt.begin());
  return DecryptedPasskey(message_type, passkey, salt);
}

}  // namespace fastpair
}  // namespace nearby
