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

#include <openssl/hkdf.h>

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#ifdef NEARBY_CHROMIUM
#include "base/check.h"
#elif defined(NEARBY_SWIFTPM)
#include "internal/platform/logging.h"
#else
#include "absl/log/check.h"  // nogncheck
#endif

#include "absl/strings/string_view.h"
#include "internal/crypto/hkdf.h"
#include "internal/crypto/hmac.h"
#include <openssl/digest.h>

namespace crypto {

std::string HkdfSha256(absl::string_view secret, absl::string_view salt,
                       absl::string_view info, size_t derived_key_size) {
  std::string key;
  key.resize(derived_key_size);
  int result = ::HKDF(
      reinterpret_cast<uint8_t*>(&key[0]), derived_key_size, EVP_sha256(),
      reinterpret_cast<const uint8_t*>(secret.data()), secret.size(),
      reinterpret_cast<const uint8_t*>(salt.data()), salt.size(),
      reinterpret_cast<const uint8_t*>(info.data()), info.size());
  DCHECK(result);
  return key;
}

std::vector<uint8_t> HkdfSha256(absl::Span<const uint8_t> secret,
                                absl::Span<const uint8_t> salt,
                                absl::Span<const uint8_t> info,
                                size_t derived_key_size) {
  std::vector<uint8_t> ret;
  ret.resize(derived_key_size);
  int result =
      ::HKDF(ret.data(), derived_key_size, EVP_sha256(), secret.data(),
             secret.size(), salt.data(), salt.size(), info.data(), info.size());
  DCHECK(result);
  return ret;
}

}  // namespace crypto
