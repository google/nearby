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

#ifndef NEARBY_UTILS_H
#define NEARBY_UTILS_H

#include "nearby_types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

uint8_t nearby_utils_GetByte(uint64_t source, int byteNumber);

void nearby_utils_CopyBigEndian(uint8_t* dest, uint64_t source, int bytes);
void nearby_utils_CopyLittleEndian(uint8_t* dest, uint64_t source, int bytes);

uint32_t nearby_utils_GetBigEndian24(uint8_t* buffer);
uint32_t nearby_utils_GetBigEndian32(uint8_t* buffer);
uint64_t nearby_utils_GetBigEndian48(uint8_t* buffer);

// Converts the data to a hex string for debugging. Returns a pointer to a
// static buffer. Don't call it twice in the same log message!
const char* nearby_utils_ArrayToString(const uint8_t* data, size_t data_length);

// Converts BT MAC address to string for debugging. Returns a pointer to a
// static buffer. Don't call it twice in the same log message!
const char* nearby_utils_MacToString(uint64_t address);

#ifdef __cplusplus
}
#endif

#endif /* NEARBY_UTILS_H */
