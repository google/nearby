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

#ifndef NEARBY_PLATFORM_TRACE_H
#define NEARBY_PLATFORM_TRACE_H

#include "nearby.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
  kTraceLevelUnknown = 0,
  kTraceLevelVerbose = 1,
  kTraceLevelDebug = 2,
  kTraceLevelInfo = 3,
  kTraceLevelWarning = 4,
  kTraceLevelError = 5,
} nearby_platform_TraceLevel;

// Generates conditional trace line. This is usually wrapped in a macro to
// provide compiler parameters.
//
// level    - Debug level, higher is less messages.
// filename - Name of file calling trace.
// lineno   - Source line of trace call.
// fmt      - printf() style format string (%).
// ...      - A series of parameters indicated by the fmt string.
void nearby_platform_Trace(nearby_platform_TraceLevel level,
                           const char *filename, int lineno, const char *fmt,
                           ...);

// Processes assert. Processes a failed assertion.
//
// filename - Name of file calling assert.
// lineno   - Source line of assert call
// reason   - String message indicating reason for assert.
void nearby_platfrom_CrashOnAssert(const char *filename, int lineno,
                                   const char *reason);

// Initializes trace module.
void nearby_platform_TraceInit(void);

#ifdef __cplusplus
}
#endif

#endif /* NEARBY_PLATFORM_TRACE_H */
