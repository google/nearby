// Copyright 2022 Google LLC
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

#include "nearby_utils.h"

uint8_t nearby_utils_GetByte(uint64_t source, int byteNumber) {
  return source >> (8 * byteNumber);
}

void nearby_utils_CopyBigEndian(uint8_t* dest, uint64_t source, int bytes) {
  int i;
  for (i = 0; i < bytes; i++) {
    dest[bytes - i - 1] = nearby_utils_GetByte(source, i);
  }
}

void nearby_utils_CopyLittleEndian(uint8_t* dest, uint64_t source, int bytes) {
  int i;
  for (i = 0; i < bytes; i++) {
    dest[i] = nearby_utils_GetByte(source, i);
  }
}

uint32_t nearby_utils_GetBigEndian24(uint8_t* buffer) {
  return (256 * 256 * buffer[0]) + (256 * buffer[1]) + buffer[2];
}

uint32_t nearby_utils_GetBigEndian32(uint8_t* buffer) {
  return (256 * 256 * 256 * buffer[0]) + (256 * 256 * buffer[1]) +
         (256 * buffer[2]) + buffer[3];
}

uint64_t nearby_utils_GetBigEndian48(uint8_t* b) {
  return (((uint64_t)b[0]) << 40) | (((uint64_t)b[1]) << 32) |
         (((uint64_t)b[2]) << 24) | (((uint64_t)b[3]) << 16) |
         (((uint64_t)b[4]) << 8) | (uint64_t)b[5];
}

static char NibbleToHex(uint8_t value) {
  if (value < 10) return '0' + value;
  value -= 10;
  if (value < 6) return 'A' + value;
  return '?';
}

const char* nearby_utils_ArrayToString(const uint8_t* data, size_t length) {
  static char buffer[80];
  size_t buffer_size = sizeof(buffer);
  size_t num_bytes;
  if (2 * length < buffer_size - 1) {
    num_bytes = 2 * length;
    buffer[num_bytes] = 0;
  } else {
    // The buffer is too small to hold all of data. Let's truncate the data and
    // append an ellipsis.
    num_bytes = buffer_size - 4;
    buffer[buffer_size - 1] = 0;
    buffer[buffer_size - 2] = '.';
    buffer[buffer_size - 3] = '.';
    buffer[buffer_size - 4] = '.';
    num_bytes = buffer_size - 4;
  }
  const uint8_t* input = data;
  char* output = buffer;
  for (size_t i = 0; i < num_bytes; i += 2) {
    uint8_t x = *input++;
    *output++ = NibbleToHex(x >> 4);
    *output++ = NibbleToHex(x & 0x0F);
  }
  return buffer;
}

const char* nearby_utils_MacToString(uint64_t address) {
  static char buffer[18];
  char* p = buffer + sizeof(buffer) - 1;
  *p-- = 0;
  while (p > buffer) {
    *p-- = NibbleToHex(address & 0xF);
    address >>= 4;
    *p-- = NibbleToHex(address & 0xF);
    address >>= 4;
    if (p >= buffer) *p-- = ':';
  }
  return buffer;
}
