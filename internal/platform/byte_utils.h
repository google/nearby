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

#include <cstdint>
#include <string>

#include "internal/platform/byte_array.h"

// TODO(edwinwu): Remove this namespace and move all the functions into
// ByteArray class.
namespace nearby::byte_utils {

// Generates a four-digit numeric string representation of a byte array.
std::string ToFourDigitString(const ByteArray& bytes);

// Converts a ByteArray to a 32-bit integer in big-endian format.
// It reads min(4, bytes.size()) bytes from input ByteArray; bytes[0] is
// read as MSB, bytes[1] as second byte, and so on. If bytes.size() < 4,
// bytes will be read to high order bytes of result and low order bytes
// will be 0.
int32_t BytesToInt(const ByteArray& bytes);

// Converts a 32-bit integer to a 4-byte ByteArray in big-endian format.
ByteArray IntToBytes(int32_t value);

}  // namespace nearby::byte_utils

#endif  // PLATFORM_BASE_BYTE_UTILS_H_
