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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SHA2_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SHA2_H_

#include <stddef.h>

#include <array>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "internal/crypto/crypto_export.h"

namespace crypto {

// These functions perform SHA-256 operations.
//
// Functions for SHA-384 and SHA-512 can be added when the need arises.

static const size_t kSHA256Length = 32;  // Length in bytes of a SHA-256 hash.

// Computes the SHA-256 hash of |input|.
CRYPTO_EXPORT std::array<uint8_t, kSHA256Length> SHA256Hash(
    absl::Span<const uint8_t> input);

// Convenience version of the above that returns the result in a 32-byte
// string.
CRYPTO_EXPORT std::string SHA256HashString(absl::string_view str);

// Computes the SHA-256 hash of the input string 'str' and stores the first
// 'len' bytes of the hash in the output buffer 'output'.  If 'len' > 32,
// only 32 bytes (the full hash) are stored in the 'output' buffer.
CRYPTO_EXPORT void SHA256HashString(absl::string_view str, void* output,
                                    size_t len);

}  // namespace crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SHA2_H_
