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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_ENCRYPTOR_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_ENCRYPTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "internal/crypto_cros/crypto_export.h"

namespace nearby::crypto {

class SymmetricKey;

// This class implements encryption without authentication, which is usually
// unsafe. Prefer crypto::Aead for new code. If using this class, prefer the
// absl::Span and std::vector overloads over the absl::string_view and
// std::string overloads.
class CRYPTO_EXPORT Encryptor {
 public:
  enum Mode {
    CBC,
    CTR,
  };

  Encryptor();
  ~Encryptor();

  // Initializes the encryptor using |key| and |iv|. Returns false if either the
  // key or the initialization vector cannot be used.
  //
  // If |mode| is CBC, |iv| must not be empty; if it is CTR, then |iv| must be
  // empty.
  bool Init(const SymmetricKey* key, Mode mode, absl::string_view iv);
  bool Init(const SymmetricKey* key, Mode mode, absl::Span<const uint8_t> iv);

  // Encrypts |plaintext| into |ciphertext|.  |plaintext| may only be empty if
  // the mode is CBC.
  bool Encrypt(absl::string_view plaintext, std::string* ciphertext);
  bool Encrypt(absl::Span<const uint8_t> plaintext,
               std::vector<uint8_t>* ciphertext);

  // Decrypts |ciphertext| into |plaintext|.  |ciphertext| must not be empty.
  //
  // WARNING: In CBC mode, Decrypt() returns false if it detects the padding
  // in the decrypted plaintext is wrong. Padding errors can result from
  // tampered ciphertext or a wrong decryption key. But successful decryption
  // does not imply the authenticity of the data. The caller of Decrypt()
  // must either authenticate the ciphertext before decrypting it, or take
  // care to not report decryption failure. Otherwise it could inadvertently
  // be used as a padding oracle to attack the crypto system.
  bool Decrypt(absl::string_view ciphertext, std::string* plaintext);
  bool Decrypt(absl::Span<const uint8_t> ciphertext,
               std::vector<uint8_t>* plaintext);

  // Sets the counter value when in CTR mode. Currently only 128-bits
  // counter value is supported.
  //
  // Returns true only if update was successful.
  bool SetCounter(absl::string_view counter);
  bool SetCounter(absl::Span<const uint8_t> counter);

  // TODO(albertb): Support streaming encryption.

 private:
  const SymmetricKey* key_;
  Mode mode_;

  bool CryptString(bool do_encrypt, absl::string_view input,
                   std::string* output);
  bool CryptBytes(bool do_encrypt, absl::Span<const uint8_t> input,
                  std::vector<uint8_t>* output);

  // On success, these helper functions return the number of bytes written to
  // |output|.
  size_t MaxOutput(bool do_encrypt, size_t length);
  absl::optional<size_t> Crypt(bool do_encrypt, absl::Span<const uint8_t> input,
                              absl::Span<uint8_t> output);
  absl::optional<size_t> CryptCTR(bool do_encrypt,
                                 absl::Span<const uint8_t> input,
                                 absl::Span<uint8_t> output);

  // In CBC mode, the IV passed to Init(). In CTR mode, the counter value passed
  // to SetCounter().
  std::vector<uint8_t> iv_;
};

}  // namespace nearby::crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_ENCRYPTOR_H_
