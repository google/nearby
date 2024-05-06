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

//*** WARNING!!! Do not add more functions and Data types to this file. ***
// This file needs to be in sync with:
// https://source.chromium.org/chromium/chromium/src/+/main:crypto/random.h

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_CHROMIUM_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_CHROMIUM_H_

#ifdef NEARBY_CHROMIUM
#error "Use chromium headers when NEARBY_CHROMIUM is defined."
#endif

#include <stddef.h>

#include <cstdint>
#include <string>

#include "internal/base/containers/span.h"
#include "internal/crypto_cros/crypto_chromium_export.h"

namespace crypto {

// Fills `bytes` with cryptographically-secure random bits.
CRYPTO_CHROMIUM_EXPORT void RandBytes(base::span<uint8_t> bytes);

}  // namespace crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_CHROMIUM_H_
