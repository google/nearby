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

#include "internal/platform/byte_utils.h"

#include <cstdlib>

#include "absl/strings/str_format.h"
#include "internal/platform/base_input_stream.h"

namespace nearby {

std::string ByteUtils::ToFourDigitString(ByteArray& bytes) {
  int multiplier = 1;
  int hashCode = 0;

  BaseInputStream base_input_stream{bytes};
  while (base_input_stream.IsAvailable(1)) {
    auto byte = static_cast<int>(base_input_stream.ReadUint8());
    hashCode = (hashCode + byte * multiplier) % kHashBasePrime;
    multiplier = multiplier * kHashBaseMultiplier % kHashBasePrime;
  }
  return absl::StrFormat("%04d", abs(hashCode));
}

}  // namespace nearby
