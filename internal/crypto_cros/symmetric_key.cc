// Copyright 2020 Google LLC
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

#include "internal/crypto_cros/symmetric_key.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#ifdef NEARBY_CHROMIUM
#include "internal/platform/logging.h"
#elif defined(NEARBY_SWIFTPM)
#include "internal/platform/logging.h"
#else
#include "absl/log/check.h"  // nogncheck
#endif

#include "internal/crypto_cros/nearby_base.h"
#include "internal/crypto_cros/openssl_util.h"
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace crypto {

namespace {

bool CheckDerivationParameters(SymmetricKey::Algorithm algorithm,
                               size_t key_size_in_bits) {
  switch (algorithm) {
    case SymmetricKey::AES:
      // Whitelist supported key sizes to avoid accidentally relying on
      // algorithms available in NSS but not BoringSSL and vice
      // versa. Note that BoringSSL does not support AES-192.
      return key_size_in_bits == 128 || key_size_in_bits == 256;
    case SymmetricKey::HMAC_SHA1:
      return key_size_in_bits % 8 == 0 && key_size_in_bits != 0;
  }

  DCHECK(false);
  return false;
}

}  // namespace

SymmetricKey::~SymmetricKey() {
  std::fill(key_.begin(), key_.end(), '\0');  // Zero out the confidential key.
}

// static
std::unique_ptr<SymmetricKey> SymmetricKey::GenerateRandomKey(
    Algorithm algorithm, size_t key_size_in_bits) {
  DCHECK_EQ(AES, algorithm);

  // Whitelist supported key sizes to avoid accidentally relying on
  // algorithms available in NSS but not BoringSSL and vice
  // versa. Note that BoringSSL does not support AES-192.
  if (key_size_in_bits != 128 && key_size_in_bits != 256) return nullptr;

  size_t key_size_in_bytes = key_size_in_bits / 8;
  DCHECK_EQ(key_size_in_bits, key_size_in_bytes * 8);

  if (key_size_in_bytes == 0) return nullptr;

  OpenSSLErrStackTracer err_tracer;
  std::unique_ptr<SymmetricKey> key(new SymmetricKey);
  uint8_t* key_data = reinterpret_cast<uint8_t*>(
      nearbybase::WriteInto(&key->key_, key_size_in_bytes + 1));

  int rv = RAND_bytes(key_data, static_cast<int>(key_size_in_bytes));
  return rv == 1 ? std::move(key) : nullptr;
}

// static
std::unique_ptr<SymmetricKey> SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
    Algorithm algorithm, const std::string& password, const std::string& salt,
    size_t iterations, size_t key_size_in_bits) {
  if (!CheckDerivationParameters(algorithm, key_size_in_bits)) return nullptr;

  size_t key_size_in_bytes = key_size_in_bits / 8;

  OpenSSLErrStackTracer err_tracer;
  std::unique_ptr<SymmetricKey> key(new SymmetricKey);
  uint8_t* key_data = reinterpret_cast<uint8_t*>(
      nearbybase::WriteInto(&key->key_, key_size_in_bytes + 1));

  int rv = PKCS5_PBKDF2_HMAC_SHA1(
      password.data(), password.length(),
      reinterpret_cast<const uint8_t*>(salt.data()), salt.length(),
      static_cast<unsigned>(iterations), key_size_in_bytes, key_data);
  return rv == 1 ? std::move(key) : nullptr;
}

// static
std::unique_ptr<SymmetricKey> SymmetricKey::DeriveKeyFromPasswordUsingScrypt(
    Algorithm algorithm, const std::string& password, const std::string& salt,
    size_t cost_parameter, size_t block_size, size_t parallelization_parameter,
    size_t max_memory_bytes, size_t key_size_in_bits) {
  if (!CheckDerivationParameters(algorithm, key_size_in_bits)) return nullptr;

  size_t key_size_in_bytes = key_size_in_bits / 8;

  OpenSSLErrStackTracer err_tracer;
  std::unique_ptr<SymmetricKey> key(new SymmetricKey);
  uint8_t* key_data = reinterpret_cast<uint8_t*>(
      nearbybase::WriteInto(&key->key_, key_size_in_bytes + 1));

  int rv = EVP_PBE_scrypt(password.data(), password.length(),
                          reinterpret_cast<const uint8_t*>(salt.data()),
                          salt.length(), cost_parameter, block_size,
                          parallelization_parameter, max_memory_bytes, key_data,
                          key_size_in_bytes);
  return rv == 1 ? std::move(key) : nullptr;
}

// static
std::unique_ptr<SymmetricKey> SymmetricKey::Import(Algorithm algorithm,
                                                   const std::string& raw_key) {
  if (algorithm == AES) {
    // Whitelist supported key sizes to avoid accidentally relying on
    // algorithms available in NSS but not BoringSSL and vice
    // versa. Note that BoringSSL does not support AES-192.
    if (raw_key.size() != 128 / 8 && raw_key.size() != 256 / 8) return nullptr;
  }

  std::unique_ptr<SymmetricKey> key(new SymmetricKey);
  key->key_ = raw_key;
  return key;
}

SymmetricKey::SymmetricKey() = default;

}  // namespace crypto
