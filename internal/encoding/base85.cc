// Copyright 2025 Google LLC
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

#include "internal/encoding/base85.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace nearby {
namespace encoding {
namespace {

constexpr int kBase85CharsNumber = 85;
constexpr uint8_t kBase85Zero = 'z';
constexpr unsigned char kBase85EncodeChars[] = {
    '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6',  '7', '8', '9', ':', ';', '<', '=', '>',
    '?', '@', 'A', 'B', 'C', 'D', 'E',  'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T',  'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\',
    ']', '^', '_', '`', 'a', 'b', 'c',  'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
    'l', 'm', 'n', 'o', 'p', 'q', 'r',  's', 't', 'u'};

constexpr uint8_t kBase85LastEncodeChar =
    kBase85EncodeChars[kBase85CharsNumber - 1];

constexpr uint8_t kBase85DecodeCharsCount[] = {0, 0, 1, 2, 3};

static uint8_t kBase85DecodeChars[256];

void InitBase85DecodeChars() {
  if (kBase85DecodeChars[kBase85LastEncodeChar]) {
    return;
  }

  std::fill(kBase85DecodeChars, kBase85DecodeChars + 256, 0xff);

  for (uint8_t i = 0; i < kBase85CharsNumber; ++i) {
    kBase85DecodeChars[kBase85EncodeChars[i]] = i;
  }
}
}  // namespace

std::string Base85Encode(const std::string& input, bool padding) {
  size_t result_size =
      5 * (input.size() >> 2) + (((input.size() % 4) * 5 + 3) >> 2);
  if (padding) {
    result_size = 5 * (input.size() >> 2) + ((input.size() % 4 != 0) ? 5 : 0);
  }

  std::string result;
  result.resize(result_size);
  char* result_buffer = result.data();
  char* result_buffer_end = result.data() + result.size();

  size_t bytes = input.size();
  const char* data = input.data();
  while (bytes) {
    int count;
    uint32_t value = 0;
    for (count = 24; count >= 0; count -= 8) {
      uint32_t ch = *data++;
      value |= ch << count;
      if (--bytes == 0) break;
    }

    if (value == 0) {
      // special case, use 'z' to encode it
      result_buffer[0] = kBase85Zero;
      ++result_buffer;
      continue;
    }

    int changed_count = 0;
    for (count = 4; count >= 0; count--) {
      int val = value % 85;
      value /= 85;
      if (result_buffer + count < result_buffer_end) {
        result_buffer[count] = kBase85EncodeChars[val];
        ++changed_count;
      }
    }

    result_buffer += changed_count;
  }

  if (result_buffer != result_buffer_end) {
    // special case, may include z encoding
    return std::string(result.data(), result_buffer - result.data());
  }

  return result;
}

std::optional<std::string> Base85Decode(const std::string& input) {
  InitBase85DecodeChars();

  size_t length = 0;
  size_t pos = 0;
  for (; pos < input.size();) {
    if (input[pos] == kBase85Zero) {
      // special zero case
      length += 4;
      ++pos;
      continue;
    }
    if (pos + 5 > input.size()) {
      size_t remain = input.size() - pos;
      if (remain == 1) {
        return std::nullopt;
      }
      length += kBase85DecodeCharsCount[remain];
      break;
    }
    pos += 5;
    length += 4;
  }

  std::string result;
  result.resize(length);

  const char* input_buffer = input.data();
  const char* input_buffer_end = input_buffer + input.size();
  char* result_buffer = result.data();
  char* result_buffer_end = result_buffer + result.size();

  while (length) {
    int count = 4;
    int decoded_char;
    uint8_t input_char;
    uint32_t value = 0;
    do {
      if (input_buffer < input_buffer_end) {
        input_char = *input_buffer++;
        // handle special case 'z'
        if (count == 4 && input_char == kBase85Zero) {
          for (int i = 0; i < count; ++i) {
            *result_buffer++ = 0x00;
          }
          length -= 4;
          continue;
        }

        decoded_char = kBase85DecodeChars[input_char];
        if (decoded_char == 0xff) return std::nullopt;
      } else {
        decoded_char = kBase85LastEncodeChar;
      }
      value = value * 85 + decoded_char;
    } while (--count);
    if (input_buffer < input_buffer_end) {
      input_char = *input_buffer++;
      decoded_char = kBase85DecodeChars[input_char];
      if (decoded_char == 0xff) return std::nullopt;
    } else {
      decoded_char = kBase85LastEncodeChar;
    }
    /* Detect overflow. */
    if (0xffffffff / 85 < value || 0xffffffff - decoded_char < (value *= 85))
      return std::nullopt;
    value += decoded_char;

    count = (length < 4) ? length : 4;
    length -= count;
    do {
      value = (value << 8) | (value >> 24);
      if (result_buffer < result_buffer_end) {
        *result_buffer++ = value;
      }
    } while (--count);
  }

  return result;
}

}  // namespace encoding
}  // namespace nearby
