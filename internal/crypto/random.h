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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_RANDOM_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_RANDOM_H_

#include <stddef.h>

#include <string>

#include "absl/types/span.h"
#include "internal/crypto/crypto_export.h"

namespace crypto {

// Fills the given buffer with |length| random bytes of cryptographically
// secure random numbers.
// |length| must be positive.
CRYPTO_EXPORT void RandBytes(void *bytes, size_t length);

// Fills |bytes| with cryptographically-secure random bits.
CRYPTO_EXPORT void RandBytes(absl::Span<uint8_t> bytes);

// Returns |length| random bytes.
CRYPTO_EXPORT std::string RandBytes(size_t length);

// Creates an object of type T initialized with random data.
// This template should be used for simple data types: int, char, etc.
template <typename T>
T RandData() {
  T data;
  RandBytes(&data, sizeof(data));
  return data;
}

}  // namespace crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_RANDOM_H_
