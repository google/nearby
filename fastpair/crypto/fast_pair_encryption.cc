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

#include "fastpair/crypto/fast_pair_encryption.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <string_view>
#include <vector>
#include <optional>

#ifdef NEARBY_CHROMIUM
#include "base/check.h"
#elif defined(NEARBY_SWIFTPM)
#include "internal/platform/logging.h"
#else
#include "absl/log/check.h"  // nogncheck
#endif

#include "fastpair/common/constant.h"
#include "fastpair/crypto/fast_pair_key_pair.h"
#include "fastpair/crypto/fast_pair_message_type.h"
#include "internal/platform/logging.h"
#include <openssl/aes.h>
#include <openssl/base.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/ecdh.h>
#include <openssl/nid.h>
#include <openssl/sha.h>

namespace nearby {
namespace fastpair {

namespace {

// Converts the public anti-spoofing key into an EC_Point.
bssl::UniquePtr<EC_POINT> GetEcPointFromPublicAntiSpoofingKey(
    const bssl::UniquePtr<EC_GROUP>& ec_group,
    std::string_view decoded_public_anti_spoofing) {
  std::array<uint8_t, kPublicKeyByteSize + 1> buffer;
  buffer[0] = POINT_CONVERSION_UNCOMPRESSED;
  std::copy(decoded_public_anti_spoofing.begin(),
            decoded_public_anti_spoofing.end(), buffer.begin() + 1);

  bssl::UniquePtr<EC_POINT> new_ec_point(EC_POINT_new(ec_group.get()));

  if (!EC_POINT_oct2point(ec_group.get(), new_ec_point.get(), buffer.data(),
                          buffer.size(), nullptr)) {
    return nullptr;
  }

  return new_ec_point;
}

// Key derivation function to be used in hashing the generated secret key.
void* KDF(const void* in, size_t inlen, void* out, size_t* outlen) {
  // Set this to 16 since that's the amount of bytes we want to use
  // for the key, even though more will be written by SHA256 below.
  *outlen = kSharedSecretKeyByteSize;
  return SHA256(static_cast<const uint8_t*>(in), inlen,
                static_cast<uint8_t*>(out));
}
}  // namespace

// TODO(b/263400788) Add unit test to cover this function and fix all Mutants
// warning
std::optional<KeyPair> FastPairEncryption::GenerateKeysWithEcdhKeyAgreement(
    std::string_view decoded_public_anti_spoofing) {
  if (decoded_public_anti_spoofing.size() != kPublicKeyByteSize) {
    NEARBY_LOGS(VERBOSE) << "Expected " << kPublicKeyByteSize
                         << " byte value for anti-spoofing key. Got:"
                         << decoded_public_anti_spoofing.size();
    return std::nullopt;
  }

  // Generate the secp256r1 key-pair.
  bssl::UniquePtr<EC_GROUP> ec_group(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));

  if (!EC_KEY_generate_key(ec_key.get())) {
    NEARBY_LOGS(VERBOSE) << __func__ << ": Failed to generate ec key";
    return std::nullopt;
  }

  // The ultimate goal here is to get a 64-byte public key. We accomplish this
  // by converting the generated public key into the uncompressed X9.62 format,
  // which is 0x04 followed by padded x and y coordinates.
  std::array<uint8_t, kPublicKeyByteSize + 1> uncompressed_private_key;
  int point_bytes_written = EC_POINT_point2oct(
      ec_group.get(), EC_KEY_get0_public_key(ec_key.get()),
      POINT_CONVERSION_UNCOMPRESSED, uncompressed_private_key.data(),
      uncompressed_private_key.size(), nullptr);

  if (point_bytes_written != uncompressed_private_key.size()) {
    NEARBY_LOGS(VERBOSE) << __func__
                         << ": EC_POINT_point2oct failed to convert public key "
                            "to uncompressed x9.62 format.";
    return std::nullopt;
  }

  bssl::UniquePtr<EC_POINT> public_anti_spoofing_point =
      GetEcPointFromPublicAntiSpoofingKey(ec_group,
                                          decoded_public_anti_spoofing);

  if (!public_anti_spoofing_point) {
    NEARBY_LOGS(VERBOSE)
        << __func__
        << ": Failed to convert Public Anti-Spoofing key to EC_POINT";
    return std::nullopt;
  }

  uint8_t secret[SHA256_DIGEST_LENGTH];
  int computed_key_size =
      ECDH_compute_key(secret, SHA256_DIGEST_LENGTH,
                       public_anti_spoofing_point.get(), ec_key.get(), &KDF);

  if (computed_key_size != kSharedSecretKeyByteSize) {
    NEARBY_LOGS(VERBOSE) << __func__ << ": ECDH_compute_key failed.";
    return std::nullopt;
  }

  // Take first 16 bytes from secret as the shared secret key.
  std::array<uint8_t, kSharedSecretKeyByteSize> shared_secret_key;
  std::copy(secret, secret + kSharedSecretKeyByteSize,
            std::begin(shared_secret_key));

  // Ignore the first byte since it is 0x04, from the above uncompressed X9 .62
  // format.
  std::array<uint8_t, kPublicKeyByteSize> public_key;
  std::copy(uncompressed_private_key.begin() + 1,
            uncompressed_private_key.end(), public_key.begin());

  return KeyPair(shared_secret_key, public_key);
}

std::array<uint8_t, kAesBlockByteSize> FastPairEncryption::EncryptBytes(
    const std::array<uint8_t, kAesBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kAesBlockByteSize>& bytes_to_encrypt) {
  AES_KEY aes_key;
  int aes_key_was_set = AES_set_encrypt_key(aes_key_bytes.data(),
                                            aes_key_bytes.size() * 8, &aes_key);
  DCHECK(aes_key_was_set == 0) << "Invalid AES key size.";
  std::array<uint8_t, kAesBlockByteSize> encrypted_bytes;
  // Bytes_to_encrypt is less than 16 bytes and can be guaranteed to be a
  // single block, so we encrypt it using AES ECB mode (Approved by: b/73360609)
  AES_encrypt(bytes_to_encrypt.data(), encrypted_bytes.data(), &aes_key);
  return encrypted_bytes;
}

}  // namespace fastpair
}  // namespace nearby
