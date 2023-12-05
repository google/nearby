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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_AEAD_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_AEAD_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "internal/crypto_cros/crypto_export.h"
#include <openssl/base.h>

namespace crypto {

// This class exposes the AES-128-CTR-HMAC-SHA256 and AES_256_GCM AEAD. Note
// that there are two versions of most methods: an historical version based
// around |StringPiece| and a more modern version that takes |absl::Span|.
// Prefer the latter in new code.
class CRYPTO_EXPORT Aead {
 public:
  enum AeadAlgorithm {
    AES_128_CTR_HMAC_SHA256,
    AES_256_GCM,
    AES_256_GCM_SIV,
    CHACHA20_POLY1305
  };

  explicit Aead(AeadAlgorithm algorithm);
  Aead(const Aead&) = delete;
  Aead& operator=(const Aead&) = delete;
  ~Aead();

  // Note that Init keeps a reference to the data pointed to by |key| thus that
  // data must outlive this object.
  void Init(absl::Span<const uint8_t> key);

  // Note that Init keeps a reference to the data pointed to by |key| thus that
  // data must outlive this object.
  void Init(const std::string* key);

  std::vector<uint8_t> Seal(absl::Span<const uint8_t> plaintext,
                            absl::Span<const uint8_t> nonce,
                            absl::Span<const uint8_t> additional_data) const;

  bool Seal(absl::string_view plaintext, absl::string_view nonce,
            absl::string_view additional_data, std::string* ciphertext) const;

  absl::optional<std::vector<uint8_t>> Open(
      absl::Span<const uint8_t> ciphertext, absl::Span<const uint8_t> nonce,
      absl::Span<const uint8_t> additional_data) const;

  bool Open(absl::string_view ciphertext, absl::string_view nonce,
            absl::string_view additional_data, std::string* plaintext) const;

  size_t KeyLength() const;

  size_t NonceLength() const;

 private:
  bool Seal(absl::Span<const uint8_t> plaintext,
            absl::Span<const uint8_t> nonce,
            absl::Span<const uint8_t> additional_data, uint8_t* out,
            size_t* output_length, size_t max_output_length) const;

  bool Open(absl::Span<const uint8_t> ciphertext,
            absl::Span<const uint8_t> nonce,
            absl::Span<const uint8_t> additional_data, uint8_t* out,
            size_t* output_length, size_t max_output_length) const;

  absl::optional<absl::Span<const uint8_t>> key_;
  const EVP_AEAD* aead_;
};

}  // namespace crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_AEAD_H_
