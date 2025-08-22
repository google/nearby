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

std::int32_t BytesToInt(const ByteArray& bytes) {
  const char* int_bytes = bytes.data();
  std::int32_t result = 0;
  result |= (static_cast<std::int32_t>(int_bytes[0]) & 0x0FF) << 24;
  result |= (static_cast<std::int32_t>(int_bytes[1]) & 0x0FF) << 16;
  result |= (static_cast<std::int32_t>(int_bytes[2]) & 0x0FF) << 8;
  result |= (static_cast<std::int32_t>(int_bytes[3]) & 0x0FF);
  return result;
}

ByteArray IntToBytes(std::int32_t value) {
  char int_bytes[sizeof(std::int32_t)];
  int_bytes[0] = static_cast<char>((value >> 24) & 0x0FF);
  int_bytes[1] = static_cast<char>((value >> 16) & 0x0FF);
  int_bytes[2] = static_cast<char>((value >> 8) & 0x0FF);
  int_bytes[3] = static_cast<char>(value & 0x0FF);
  return ByteArray(int_bytes, sizeof(int_bytes));
}

}  // namespace byte_utils
}  // namespace nearby
