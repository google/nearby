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

#include <openssl/hmac.h>

#include <stddef.h>

#include <algorithm>
#include <string>

#ifdef NEARBY_CHROMIUM
#include "base/check.h"
#elif defined(NEARBY_SWIFTPM)
#include "internal/platform/logging.h"
#else
#include "absl/log/check.h"  // nogncheck
#endif

#include "internal/crypto/hmac.h"
#include "internal/crypto/nearby_base.h"
#include "internal/crypto/openssl_util.h"
#include "internal/crypto/secure_util.h"
#include "internal/crypto/symmetric_key.h"

namespace crypto {

HMAC::HMAC(HashAlgorithm hash_alg) : hash_alg_(hash_alg), initialized_(false) {
  // Only SHA-1 and SHA-256 hash algorithms are supported now.
  DCHECK(hash_alg_ == SHA1 || hash_alg_ == SHA256);
}

HMAC::~HMAC() {
  // Zero out key copy.
  key_.assign(key_.size(), 0);
  nearbybase::STLClearObject(&key_);
}

size_t HMAC::DigestLength() const {
  switch (hash_alg_) {
    case SHA1:
      return 20;
    case SHA256:
      return 32;
    default:
      DCHECK(false);
      return 0;
  }
}

bool HMAC::Init(const unsigned char* key, size_t key_length) {
  // Init must not be called more than once on the same HMAC object.
  DCHECK(!initialized_);
  initialized_ = true;
  key_.assign(key, key + key_length);
  return true;
}

bool HMAC::Init(const SymmetricKey* key) { return Init(key->key()); }

bool HMAC::Sign(absl::string_view data, unsigned char* digest,
                size_t digest_length) const {
  return Sign(nearbybase::as_bytes(absl::MakeSpan(data)),
              absl::MakeSpan(digest, digest_length));
}

bool HMAC::Sign(absl::Span<const uint8_t> data,
                absl::Span<uint8_t> digest) const {
  DCHECK(initialized_);

  if (digest.size() > DigestLength()) return false;

  ScopedOpenSSLSafeSizeBuffer<EVP_MAX_MD_SIZE> result(digest.data(),
                                                      digest.size());
  return !!::HMAC(hash_alg_ == SHA1 ? EVP_sha1() : EVP_sha256(), key_.data(),
                  key_.size(), data.data(), data.size(), result.safe_buffer(),
                  nullptr);
}

bool HMAC::Verify(absl::string_view data, absl::string_view digest) const {
  return Verify(nearbybase::as_bytes(absl::MakeSpan(data)),
                nearbybase::as_bytes(absl::MakeSpan(digest)));
}

bool HMAC::Verify(absl::Span<const uint8_t> data,
                  absl::Span<const uint8_t> digest) const {
  if (digest.size() != DigestLength()) return false;
  return VerifyTruncated(data, digest);
}

bool HMAC::VerifyTruncated(absl::string_view data,
                           absl::string_view digest) const {
  return VerifyTruncated(nearbybase::as_bytes(absl::MakeSpan(data)),
                         nearbybase::as_bytes(absl::MakeSpan(digest)));
}

bool HMAC::VerifyTruncated(absl::Span<const uint8_t> data,
                           absl::Span<const uint8_t> digest) const {
  if (digest.empty()) return false;

  size_t digest_length = DigestLength();
  if (digest.size() > digest_length) return false;

  uint8_t computed_digest[EVP_MAX_MD_SIZE];
  CHECK_LE(digest.size(), size_t{EVP_MAX_MD_SIZE});
  if (!Sign(data, absl::MakeSpan(computed_digest, digest.size()))) return false;

  return SecureMemEqual(digest.data(), computed_digest, digest.size());
}

}  // namespace crypto
