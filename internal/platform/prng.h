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

#ifndef PLATFORM_BASE_PRNG_H_
#define PLATFORM_BASE_PRNG_H_

#include <cstdint>

namespace nearby {

// A (non-cryptographic) pseudo-random number generator.
class Prng {
 public:
  Prng();
  ~Prng();

  std::int32_t NextInt32();
  std::uint32_t NextUint32();
  std::int64_t NextInt64();
};

}  // namespace nearby

#endif  // PLATFORM_BASE_PRNG_H_
