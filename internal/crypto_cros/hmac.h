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

// Utility class for calculating the HMAC for a given message. We currently only
// support SHA-1 and SHA-256 for the hash algorithm, but this can be extended
// easily. Prefer the absl::Span and std::vector overloads over the
// absl::string_view and std::string overloads.

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_HMAC_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_HMAC_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "internal/crypto_cros/crypto_export.h"
#include "internal/crypto_cros/nearby_base.h"

namespace nearby::crypto {

// Simplify the interface and reduce includes by abstracting out the internals.
class SymmetricKey;

class CRYPTO_EXPORT HMAC {
 public:
  // The set of supported hash functions. Extend as required.
  enum HashAlgorithm {
    SHA1,
    SHA256,
  };

  explicit HMAC(HashAlgorithm hash_alg);

  HMAC(const HMAC&) = delete;
  HMAC& operator=(const HMAC&) = delete;

  ~HMAC();

  // Returns the length of digest that this HMAC will create.
  size_t DigestLength() const;

  // TODO(abarth): Add a PreferredKeyLength() member function.

  // Initializes this instance using |key| of the length |key_length|. Call Init
  // only once. It returns false on the second or later calls.
  //
  // NOTE: the US Federal crypto standard FIPS 198, Section 3 says:
  //   The size of the key, K, shall be equal to or greater than L/2, where L
  //   is the size of the hash function output.
  // In FIPS 198-1 (and SP-800-107, which describes key size recommendations),
  // this requirement is gone.  But a system crypto library may still enforce
  // this old requirement.  If the key is shorter than this recommended value,
  // Init() may fail.
  ABSL_MUST_USE_RESULT bool Init(const unsigned char* key, size_t key_length);

  // Initializes this instance using |key|. Call Init
  // only once. It returns false on the second or later calls.
  ABSL_MUST_USE_RESULT bool Init(const SymmetricKey* key);

  // Initializes this instance using |key|. Call Init only once. It returns
  // false on the second or later calls.
  ABSL_MUST_USE_RESULT bool Init(absl::string_view key) {
    return Init(nearbybase::as_bytes(absl::MakeSpan(key)));
  }

  // Initializes this instance using |key|. Call Init only once. It returns
  // false on the second or later calls.
  ABSL_MUST_USE_RESULT bool Init(absl::Span<const uint8_t> key) {
    return Init(key.data(), key.size());
  }

  // Calculates the HMAC for the message in |data| using the algorithm supplied
  // to the constructor and the key supplied to the Init method. The HMAC is
  // returned in |digest|, which has |digest_length| bytes of storage available.
  // If |digest_length| is smaller than DigestLength(), the output will be
  // truncated. If it is larger, this method will fail.
  ABSL_MUST_USE_RESULT bool Sign(absl::string_view data, unsigned char* digest,
                                 size_t digest_length) const;
  ABSL_MUST_USE_RESULT bool Sign(absl::Span<const uint8_t> data,
                                 absl::Span<uint8_t> digest) const;

  // Verifies that the HMAC for the message in |data| equals the HMAC provided
  // in |digest|, using the algorithm supplied to the constructor and the key
  // supplied to the Init method. Use of this method is strongly recommended
  // over using Sign() with a manual comparison (such as memcmp), as such
  // comparisons may result in side-channel disclosures, such as timing, that
  // undermine the cryptographic integrity. |digest| must be exactly
  // |DigestLength()| bytes long.
  ABSL_MUST_USE_RESULT bool Verify(absl::string_view data,
                                   absl::string_view digest) const;
  ABSL_MUST_USE_RESULT bool Verify(absl::Span<const uint8_t> data,
                                   absl::Span<const uint8_t> digest) const;

  // Verifies a truncated HMAC, behaving identical to Verify(), except
  // that |digest| is allowed to be smaller than |DigestLength()|.
  ABSL_MUST_USE_RESULT bool VerifyTruncated(absl::string_view data,
                                            absl::string_view digest) const;
  ABSL_MUST_USE_RESULT bool VerifyTruncated(
      absl::Span<const uint8_t> data,
      absl::Span<const uint8_t> digest) const ABSL_MUST_USE_RESULT;

 private:
  HashAlgorithm hash_alg_;
  bool initialized_;
  std::vector<unsigned char> key_;
};

}  // namespace nearby::crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_HMAC_H_
