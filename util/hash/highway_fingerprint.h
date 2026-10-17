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
#ifndef UTIL_HASH_HIGHWAY_FINGERPRINT_H_
#define UTIL_HASH_HIGHWAY_FINGERPRINT_H_

#include <cstdint>

#include "absl/strings/string_view.h"

namespace util_hash {

// Stand-in for the Google-internal HighwayHash fingerprint. The only caller
// derives the id of a credential uploaded to the Nearby identity service,
// which this build never reaches, so any stable 64-bit hash serves. This is
// 64-bit FNV-1a, not HighwayHash.
inline uint64_t HighwayFingerprint64(absl::string_view data) {
  uint64_t hash = 0xcbf29ce484222325ULL;
  for (unsigned char byte : data) {
    hash ^= byte;
    hash *= 0x100000001b3ULL;
  }
  return hash;
}

}  // namespace util_hash

#endif  // UTIL_HASH_HIGHWAY_FINGERPRINT_H_
