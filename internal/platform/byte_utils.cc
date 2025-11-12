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

#include <cstdint>
#include <cstdlib>
#include <string>

#include "absl/strings/str_format.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/stream_reader.h"

namespace nearby {
namespace byte_utils {

namespace {
// The biggest prime number under 10000, used as a mod base to trim integers
// into 4 digits.
constexpr int kHashBasePrime = 9973;

// The hash multiplier.
constexpr int kHashBaseMultiplier = 31;
}  // namespace

std::string ToFourDigitString(const ByteArray& bytes) {
  int multiplier = 1;
  int hashCode = 0;

  StreamReader stream_reader{&bytes};
  while (stream_reader.IsAvailable(1)) {
    auto byte = stream_reader.ReadInt8().value_or(0);
    hashCode = (hashCode + byte * multiplier) % kHashBasePrime;
    multiplier = multiplier * kHashBaseMultiplier % kHashBasePrime;
  }
  return absl::StrFormat("%04d", abs(hashCode));
}

int32_t BytesToInt(const ByteArray& bytes) {
  const char* int_bytes = bytes.data();
  int32_t result = 0;
  for (int i = 0; i < bytes.size() && i < 4; ++i) {
    result <<= 8;
    result |= static_cast<int32_t>(int_bytes[i]) & 0x0FF;
  }
  return result;
}

ByteArray IntToBytes(int32_t value) {
  ByteArray result(sizeof(int32_t));
  char* buffer = result.data();
  buffer[0] = static_cast<char>((value >> 24) & 0x0FF);
  buffer[1] = static_cast<char>((value >> 16) & 0x0FF);
  buffer[2] = static_cast<char>((value >> 8) & 0x0FF);
  buffer[3] = static_cast<char>(value & 0x0FF);
  return result;
}

}  // namespace byte_utils
}  // namespace nearby
