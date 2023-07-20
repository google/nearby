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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_HKDF_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_HKDF_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "internal/crypto_cros/crypto_export.h"

namespace crypto {

CRYPTO_EXPORT
std::string HkdfSha256(absl::string_view secret, absl::string_view salt,
                       absl::string_view info, size_t derived_key_size);

CRYPTO_EXPORT
std::vector<uint8_t> HkdfSha256(absl::Span<const uint8_t> secret,
                                absl::Span<const uint8_t> salt,
                                absl::Span<const uint8_t> info,
                                size_t derived_key_size);

}  // namespace crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_HKDF_H_
