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

#ifndef PLATFORM_BASE_BYTE_UTILS_H_
#define PLATFORM_BASE_BYTE_UTILS_H_

#include <string>
#include "internal/platform/byte_array.h"

namespace nearby {

class ByteUtils {
 public:
  static std::string ToFourDigitString(ByteArray& bytes);

 private:
  // The biggest prime number under 10000, used as a mod base to trim integers
  // into 4 digits.
  static constexpr int kHashBasePrime = 9973;
  // The hash multiplier.
  static constexpr int kHashBaseMultiplier = 31;
};

}  // namespace nearby

#endif  // PLATFORM_BASE_BYTE_UTILS_H_
