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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_EC_PRIVATE_KEY_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_EC_PRIVATE_KEY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "internal/crypto/crypto_export.h"
#include <openssl/base.h>

namespace crypto {

// Encapsulates an elliptic curve (EC) private key. Can be used to generate new
// keys, export keys to other formats, or to extract a public key.
// TODO(mattm): make this and RSAPrivateKey implement some PrivateKey interface.
// (The difference in types of key() and public_key() make this a little
// tricky.)
class CRYPTO_EXPORT ECPrivateKey {
 public:
  ECPrivateKey(const ECPrivateKey&) = delete;
  ECPrivateKey& operator=(const ECPrivateKey&) = delete;

  ~ECPrivateKey();

  // Creates a new random instance. Can return nullptr if initialization fails.
  // The created key will use the NIST P-256 curve.
  // TODO(mattm): Add a curve parameter.
  static std::unique_ptr<ECPrivateKey> Create();

  // Create a new instance by importing an existing private key. The format is
  // an ASN.1-encoded PrivateKeyInfo block from PKCS #8. This can return
  // nullptr if initialization fails.
  static std::unique_ptr<ECPrivateKey> CreateFromPrivateKeyInfo(
      absl::Span<const uint8_t> input);

  // Creates a new instance by importing an existing key pair.
  // The key pair is given as an ASN.1-encoded PKCS #8 EncryptedPrivateKeyInfo
  // block with empty password and an X.509 SubjectPublicKeyInfo block.
  // Returns nullptr if initialization fails.
  //
  // This function is deprecated. Use CreateFromPrivateKeyInfo for new code.
  // See https://crbug.com/603319.
  static std::unique_ptr<ECPrivateKey> CreateFromEncryptedPrivateKeyInfo(
      absl::Span<const uint8_t> encrypted_private_key_info);

  // Returns a copy of the object.
  std::unique_ptr<ECPrivateKey> Copy() const;

  EVP_PKEY* key() { return key_.get(); }

  // Exports the private key to a PKCS #8 PrivateKeyInfo block.
  bool ExportPrivateKey(std::vector<uint8_t>* output) const;

  // Exports the private key as an ASN.1-encoded PKCS #8 EncryptedPrivateKeyInfo
  // block wth empty password. This was historically used as a workaround for
  // NSS API deficiencies and does not provide security.
  //
  // This function is deprecated. Use ExportPrivateKey for new code. See
  // https://crbug.com/603319.
  bool ExportEncryptedPrivateKey(std::vector<uint8_t>* output) const;

  // Exports the public key to an X.509 SubjectPublicKeyInfo block.
  bool ExportPublicKey(std::vector<uint8_t>* output) const;

  // Exports the public key as an EC point in X9.62 uncompressed form. Note this
  // includes the leading 0x04 byte.
  bool ExportRawPublicKey(std::string* output) const;

 private:
  // Constructor is private. Use one of the Create*() methods above instead.
  ECPrivateKey();

  bssl::UniquePtr<EVP_PKEY> key_;
};

}  // namespace crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_EC_PRIVATE_KEY_H_
