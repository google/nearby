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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_RSA_PRIVATE_KEY_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_RSA_PRIVATE_KEY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "internal/crypto_cros/crypto_export.h"
#include <openssl/base.h>

namespace crypto {

// Encapsulates an RSA private key. Can be used to generate new keys, export
// keys to other formats, or to extract a public key.
// TODO(hclam): This class should be ref-counted so it can be reused easily.
class CRYPTO_EXPORT RSAPrivateKey {
 public:
  RSAPrivateKey(const RSAPrivateKey&) = delete;
  RSAPrivateKey& operator=(const RSAPrivateKey&) = delete;

  ~RSAPrivateKey();

  // Create a new random instance. Can return NULL if initialization fails.
  static std::unique_ptr<RSAPrivateKey> Create(uint16_t num_bits);

  // Create a new instance by importing an existing private key. The format is
  // an ASN.1-encoded PrivateKeyInfo block from PKCS #8. This can return NULL if
  // initialization fails.
  static std::unique_ptr<RSAPrivateKey> CreateFromPrivateKeyInfo(
      absl::Span<const uint8_t> input);

  // Create a new instance from an existing EVP_PKEY, taking a
  // reference to it. |key| must be an RSA key. Returns NULL on
  // failure.
  static std::unique_ptr<RSAPrivateKey> CreateFromKey(EVP_PKEY* key);

  EVP_PKEY* key() const { return key_.get(); }

  // Creates a copy of the object.
  std::unique_ptr<RSAPrivateKey> Copy() const;

  // Exports the private key to a PKCS #8 PrivateKeyInfo block.
  bool ExportPrivateKey(std::vector<uint8_t>* output) const;

  // Exports the public key to an X509 SubjectPublicKeyInfo block.
  bool ExportPublicKey(std::vector<uint8_t>* output) const;

 private:
  // Constructor is private. Use one of the Create*() methods above instead.
  RSAPrivateKey();

  bssl::UniquePtr<EVP_PKEY> key_;
};

}  // namespace crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_RSA_PRIVATE_KEY_H_
