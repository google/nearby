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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SYMMETRIC_KEY_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SYMMETRIC_KEY_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "internal/crypto_cros/crypto_export.h"

namespace nearby::crypto {

// Wraps a platform-specific symmetric key and allows it to be held in a
// scoped_ptr.
class CRYPTO_EXPORT SymmetricKey {
 public:
  // Defines the algorithm that a key will be used with. See also
  // classs Encrptor.
  enum Algorithm {
    AES,
    HMAC_SHA1,
  };

  SymmetricKey(const SymmetricKey&) = delete;
  SymmetricKey& operator=(const SymmetricKey&) = delete;

  virtual ~SymmetricKey();

  // Generates a random key suitable to be used with |algorithm| and of
  // |key_size_in_bits| bits. |key_size_in_bits| must be a multiple of 8.
  // The caller is responsible for deleting the returned SymmetricKey.
  static std::unique_ptr<SymmetricKey> GenerateRandomKey(
      Algorithm algorithm, size_t key_size_in_bits);

  // Derives a key from the supplied password and salt using PBKDF2, suitable
  // for use with specified |algorithm|. Note |algorithm| is not the algorithm
  // used to derive the key from the password. |key_size_in_bits| must be a
  // multiple of 8. The caller is responsible for deleting the returned
  // SymmetricKey.
  static std::unique_ptr<SymmetricKey> DeriveKeyFromPasswordUsingPbkdf2(
      Algorithm algorithm, const std::string& password, const std::string& salt,
      size_t iterations, size_t key_size_in_bits);

  // Derives a key from the supplied password and salt using scrypt, suitable
  // for use with specified |algorithm|. Note |algorithm| is not the algorithm
  // used to derive the key from the password. |cost_parameter|, |block_size|,
  // and |parallelization_parameter| correspond to the parameters |N|, |r|, and
  // |p| from the scrypt specification (see RFC 7914). |key_size_in_bits| must
  // be a multiple of 8. The caller is responsible for deleting the returned
  // SymmetricKey.
  static std::unique_ptr<SymmetricKey> DeriveKeyFromPasswordUsingScrypt(
      Algorithm algorithm, const std::string& password, const std::string& salt,
      size_t cost_parameter, size_t block_size,
      size_t parallelization_parameter, size_t max_memory_bytes,
      size_t key_size_in_bits);

  // Imports an array of key bytes in |raw_key|. This key may have been
  // generated by GenerateRandomKey or DeriveKeyFromPassword{Pbkdf2,Scrypt} and
  // exported with key(). The key must be of suitable size for use with
  // |algorithm|. The caller owns the returned SymmetricKey.
  static std::unique_ptr<SymmetricKey> Import(Algorithm algorithm,
                                              const std::string& raw_key);

  // Returns the raw platform specific key data.
  const std::string& key() const { return key_; }

 private:
  SymmetricKey();

  std::string key_;
};

}  // namespace nearby::crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SYMMETRIC_KEY_H_
