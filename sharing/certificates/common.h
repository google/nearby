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

#ifndef THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_COMMON_H_
#define THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_COMMON_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/crypto_cros/encryptor.h"
#include "internal/crypto_cros/symmetric_key.h"

namespace nearby {
namespace sharing {

// Returns true if the |current_time| exceeds |not_after| by more than the
// public certificate clock-skew tolerance if applicable.
bool IsNearbyShareCertificateExpired(absl::Time current_time,
                                     absl::Time not_after,
                                     bool use_public_certificate_tolerance);

// Returns true if the |current_time| is in the interval
// [|not_before| - tolerance, |not_after| + tolerance), where a clock-skew
// tolerance is only non-zero if |use_public_certificate_tolerance| is true.
bool IsNearbyShareCertificateWithinValidityPeriod(
    absl::Time current_time, absl::Time not_before, absl::Time not_after,
    bool use_public_certificate_tolerance);

// Uses HKDF to create a hash of the |authentication_token|, using the
// |secret_key|. A trivial info parameter is used, and the output length is
// fixed to be kNearbyShareNumBytesAuthenticationTokenHash to conform with the
// GmsCore implementation.
std::vector<uint8_t> ComputeAuthenticationTokenHash(
    absl::Span<const uint8_t> authentication_token,
    absl::Span<const uint8_t> secret_key);

// Uses HKDF to generate a new key of length |new_num_bytes| from |key|. To
// conform with the GmsCore implementation, trivial salt and info are used.
std::vector<uint8_t> DeriveNearbyShareKey(absl::Span<const uint8_t> key,
                                          size_t new_num_bytes);

// Generates a random byte array with size |num_bytes|.
std::vector<uint8_t> GenerateRandomBytes(size_t num_bytes);

// Creates a CTR Encryptor used for metadata key encryption/decryption.
std::unique_ptr<crypto::Encryptor> CreateNearbyShareCtrEncryptor(
    const crypto::SymmetricKey* secret_key, absl::Span<const uint8_t> salt);

// Generates Time from JAVA Time
absl::Time FromJavaTime(int64_t ms_since_epoch);
int64_t ToJavaTime(absl::Time time);

template <typename T>
absl::Span<const uint8_t> as_bytes(absl::Span<T> s) noexcept {
  return {reinterpret_cast<const uint8_t*>(s.data()), s.size() * sizeof(T)};
}

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_COMMON_H_
