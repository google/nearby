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

#ifndef NEARBY_ASSERT_H
#define NEARBY_ASSERT_H

#include "nearby.h"
#include "nearby_platform_trace.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __NEARBY_SHORT_FILE__
#define NEARBY_ASSERT(cond)                                                  \
  do {                                                                       \
    if (!(cond)) {                                                           \
      nearby_platfrom_CrashOnAssert(__NEARBY_SHORT_FILE__, __LINE__, #cond); \
    }                                                                        \
  } while (0)
#else
#define NEARBY_ASSERT(cond)                                     \
  do {                                                          \
    if (!(cond)) {                                              \
      nearby_platfrom_CrashOnAssert(__FILE__, __LINE__, #cond); \
    }                                                           \
  } while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* NEARBY_ASSERT_H */
