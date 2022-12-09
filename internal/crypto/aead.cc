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

#include "internal/crypto/aead.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#ifdef NEARBY_CHROMIUM
#include "base/check.h"
#elif defined(NEARBY_SWIFTPM)
#include "internal/platform/logging.h"
#else
#include "absl/log/check.h"  // nogncheck
#endif

#include "absl/types/span.h"
#include "internal/crypto/nearby_base.h"
#include "internal/crypto/openssl_util.h"
#include <openssl/aes.h>
#include <openssl/evp.h>

namespace crypto {

Aead::Aead(AeadAlgorithm algorithm) {
  EnsureOpenSSLInit();
  switch (algorithm) {
    case AES_128_CTR_HMAC_SHA256:
      aead_ = EVP_aead_aes_128_ctr_hmac_sha256();
      break;
    case AES_256_GCM:
      aead_ = EVP_aead_aes_256_gcm();
      break;
    case AES_256_GCM_SIV:
      aead_ = EVP_aead_aes_256_gcm_siv();
      break;
    case CHACHA20_POLY1305:
      aead_ = EVP_aead_chacha20_poly1305();
      break;
  }
}

Aead::~Aead() = default;

void Aead::Init(absl::Span<const uint8_t> key) {
  DCHECK(!key_);
  DCHECK_EQ(KeyLength(), key.size());
  key_ = key;
}

static absl::Span<const uint8_t> ToSpan(absl::string_view sp) {
  return nearbybase::as_bytes(absl::MakeSpan(sp));
}

void Aead::Init(const std::string* key) { Init(ToSpan(*key)); }

std::vector<uint8_t> Aead::Seal(
    absl::Span<const uint8_t> plaintext, absl::Span<const uint8_t> nonce,
    absl::Span<const uint8_t> additional_data) const {
  const size_t max_output_length =
      EVP_AEAD_max_overhead(aead_) + plaintext.size();
  CHECK(max_output_length >= plaintext.size());
  std::vector<uint8_t> ret;
  ret.resize(max_output_length);

  size_t output_length;
  CHECK(Seal(plaintext, nonce, additional_data, ret.data(), &output_length,
             max_output_length));
  ret.resize(output_length);
  return ret;
}

bool Aead::Seal(absl::string_view plaintext, absl::string_view nonce,
                absl::string_view additional_data,
                std::string* ciphertext) const {
  const size_t max_output_length =
      EVP_AEAD_max_overhead(aead_) + plaintext.size();
  CHECK(max_output_length + 1 >= plaintext.size());
  uint8_t* out_ptr = reinterpret_cast<uint8_t*>(
      nearbybase::WriteInto(ciphertext, max_output_length + 1));

  size_t output_length;
  if (!Seal(ToSpan(plaintext), ToSpan(nonce), ToSpan(additional_data), out_ptr,
            &output_length, max_output_length)) {
    ciphertext->clear();
    return false;
  }

  ciphertext->resize(output_length);
  return true;
}

absl::optional<std::vector<uint8_t>> Aead::Open(
    absl::Span<const uint8_t> ciphertext, absl::Span<const uint8_t> nonce,
    absl::Span<const uint8_t> additional_data) const {
  const size_t max_output_length = ciphertext.size();
  std::vector<uint8_t> ret;
  ret.resize(max_output_length);

  size_t output_length;
  if (!Open(ciphertext, nonce, additional_data, ret.data(), &output_length,
            max_output_length)) {
    return absl::nullopt;
  }

  ret.resize(output_length);
  return ret;
}

bool Aead::Open(absl::string_view ciphertext, absl::string_view nonce,
                absl::string_view additional_data,
                std::string* plaintext) const {
  const size_t max_output_length = ciphertext.size();
  CHECK(max_output_length + 1 > max_output_length);
  uint8_t* out_ptr = reinterpret_cast<uint8_t*>(
      nearbybase::WriteInto(plaintext, max_output_length + 1));

  size_t output_length;
  if (!Open(ToSpan(ciphertext), ToSpan(nonce), ToSpan(additional_data), out_ptr,
            &output_length, max_output_length)) {
    plaintext->clear();
    return false;
  }

  plaintext->resize(output_length);
  return true;
}

size_t Aead::KeyLength() const { return EVP_AEAD_key_length(aead_); }

size_t Aead::NonceLength() const { return EVP_AEAD_nonce_length(aead_); }

bool Aead::Seal(absl::Span<const uint8_t> plaintext,
                absl::Span<const uint8_t> nonce,
                absl::Span<const uint8_t> additional_data, uint8_t* out,
                size_t* output_length, size_t max_output_length) const {
  DCHECK(key_);
  DCHECK_EQ(NonceLength(), nonce.size());
  bssl::ScopedEVP_AEAD_CTX ctx;

  if (!EVP_AEAD_CTX_init(ctx.get(), aead_, key_->data(), key_->size(),
                         EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr) ||
      !EVP_AEAD_CTX_seal(ctx.get(), out, output_length, max_output_length,
                         nonce.data(), nonce.size(), plaintext.data(),
                         plaintext.size(), additional_data.data(),
                         additional_data.size())) {
    return false;
  }

  DCHECK_LE(*output_length, max_output_length);
  return true;
}

bool Aead::Open(absl::Span<const uint8_t> plaintext,
                absl::Span<const uint8_t> nonce,
                absl::Span<const uint8_t> additional_data, uint8_t* out,
                size_t* output_length, size_t max_output_length) const {
  DCHECK(key_);
  DCHECK_EQ(NonceLength(), nonce.size());
  bssl::ScopedEVP_AEAD_CTX ctx;

  if (!EVP_AEAD_CTX_init(ctx.get(), aead_, key_->data(), key_->size(),
                         EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr) ||
      !EVP_AEAD_CTX_open(ctx.get(), out, output_length, max_output_length,
                         nonce.data(), nonce.size(), plaintext.data(),
                         plaintext.size(), additional_data.data(),
                         additional_data.size())) {
    return false;
  }

  DCHECK_LE(*output_length, max_output_length);
  return true;
}

}  // namespace crypto
