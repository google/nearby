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

#ifndef NEARBY_PLATFORM_PERSISTENCE_H
#define NEARBY_PLATFORM_PERSISTENCE_H

#include "nearby.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
  kStoredKeyAccountKeyList,
  kStoredKeyPersonalizedName
} nearby_fp_StoredKey;

// Loads stored key
//
// key    - Type of key to fetch.
// output - Buffer to contain retrieved key.
// length - On input, contains the size of the output buffer.
//          On output, contains the Length of key.
nearby_platform_status nearby_platform_LoadValue(nearby_fp_StoredKey key,
                                                 uint8_t* output,
                                                 size_t* length);

// Saves stored key
//
// key    - Type of key to store.
// output - Buffer containing key to store.
// length - Length of key.
nearby_platform_status nearby_platform_SaveValue(nearby_fp_StoredKey key,
                                                 const uint8_t* input,
                                                 size_t length);

// Initializes persistence module
nearby_platform_status nearby_platform_PersistenceInit();

#ifdef __cplusplus
}
#endif

#endif /* NEARBY_PLATFORM_PERSISTENCE_H */
