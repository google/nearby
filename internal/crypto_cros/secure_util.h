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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SECURE_UTIL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SECURE_UTIL_H_

#include <stddef.h>

#include "internal/crypto_cros/crypto_export.h"

namespace nearby::crypto {

// Performs a constant-time comparison of two strings, returning true if the
// strings are equal.
//
// For cryptographic operations, comparison functions such as memcmp() may
// expose side-channel information about input, allowing an attacker to
// perform timing analysis to determine what the expected bits should be. In
// order to avoid such attacks, the comparison must execute in constant time,
// so as to not to reveal to the attacker where the difference(s) are.
// For an example attack, see
// http://groups.google.com/group/keyczar-discuss/browse_thread/thread/5571eca0948b2a13
CRYPTO_EXPORT bool SecureMemEqual(const void* s1, const void* s2, size_t n);

}  // namespace nearby::crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SECURE_UTIL_H_
