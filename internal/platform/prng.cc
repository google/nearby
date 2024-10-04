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

#include "internal/platform/prng.h"

#include <cstdint>
#include <cstdlib>
#include <limits>

#include "absl/time/clock.h"

namespace nearby {

#define UNSIGNED_INT_BITMASK (std::numeric_limits<unsigned int>::max())

Prng::Prng() {
  // absl::GetCurrentTimeNanos() returns 64 bits, but srand() wants an unsigned
  // int, so we may have to lose some of those 64 bits.
  //
  // The lower bits of the current-time-in-nanos are likely to have more entropy
  // than the upper bits, so choose the former.
  srand(static_cast<unsigned int>(absl::GetCurrentTimeNanos() &
                                  UNSIGNED_INT_BITMASK));
}

Prng::~Prng() {
  // Nothing to do.
}

#define RANDOM_BYTE (rand() & 0x0FF)  // NOLINT

std::int32_t Prng::NextInt32() {
  return (static_cast<std::int32_t>(RANDOM_BYTE) << 24) |
         (static_cast<std::int32_t>(RANDOM_BYTE) << 16) |
         (static_cast<std::int32_t>(RANDOM_BYTE) << 8) |
         (static_cast<std::int32_t>(RANDOM_BYTE));
}

std::uint32_t Prng::NextUint32() {
  return static_cast<std::uint32_t>(NextInt32());
}

std::int64_t Prng::NextInt64() {
  return (static_cast<std::int64_t>(NextInt32()) << 32) |
         (static_cast<std::int64_t>(NextUint32()));
}

}  // namespace nearby
