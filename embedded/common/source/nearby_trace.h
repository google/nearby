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

#ifndef NEARBY_TRACE_H
#define NEARBY_TRACE_H

#include "nearby.h"
#include "nearby_platform_trace.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Must be kept in sync with TraceLevel in nearby_platform_trace.h
#define NEARBY_TRACE_LEVEL_VERBOSE (1)
#define NEARBY_TRACE_LEVEL_DEBUG (2)
#define NEARBY_TRACE_LEVEL_INFO (3)
#define NEARBY_TRACE_LEVEL_WARNING (4)
#define NEARBY_TRACE_LEVEL_ERROR (5)
#define NEARBY_TRACE_LEVEL_OFF (6)

#if NEARBY_TRACE_LEVEL < NEARBY_TRACE_LEVEL_VERBOSE || \
    NEARBY_TRACE_LEVEL > NEARBY_TRACE_LEVEL_OFF
#error "Invalid definition of NEARBY_TRACE_LEVEL."
#endif

#if NEARBY_TRACE_LEVEL > NEARBY_TRACE_LEVEL_VERBOSE
#define NEARBY_TRACE_VERBOSE(level, ...)
#else
#define NEARBY_TRACE_VERBOSE(level, ...) _NEARBY_TRACE(level, __VA_ARGS__)
#define NEARBY_ALL_ENUM_TO_STR_ENABLE 1
#endif

#if NEARBY_TRACE_LEVEL > NEARBY_TRACE_LEVEL_DEBUG
#define NEARBY_TRACE_DEBUG(level, ...)
#else
#define NEARBY_TRACE_DEBUG(level, ...) _NEARBY_TRACE(level, __VA_ARGS__)
#endif

#if NEARBY_TRACE_LEVEL > NEARBY_TRACE_LEVEL_INFO
#define NEARBY_TRACE_INFO(level, ...)
#else
#define NEARBY_TRACE_INFO(level, ...) _NEARBY_TRACE(level, __VA_ARGS__)
#endif

#if NEARBY_TRACE_LEVEL > NEARBY_TRACE_LEVEL_WARNING
#define NEARBY_TRACE_WARNING(level, ...)
#else
#define NEARBY_TRACE_WARNING(level, ...) _NEARBY_TRACE(level, __VA_ARGS__)
#endif

#if NEARBY_TRACE_LEVEL > NEARBY_TRACE_LEVEL_ERROR
#define NEARBY_TRACE_ERROR(level, ...)
#else
#define NEARBY_TRACE_ERROR(level, ...) _NEARBY_TRACE(level, __VA_ARGS__)
#endif

#ifndef __NEARBY_SHORT_FILE__
#define __NEARBY_SHORT_FILE__ __FILE__
#endif

#define _NEARBY_TRACE(level, ...)                               \
  nearby_platform_Trace(level, __NEARBY_SHORT_FILE__, __LINE__, \
                        "@g " __VA_ARGS__);

#define NEARBY_TRACE(level, ...)                                               \
  NEARBY_TRACE_##level((nearby_platform_TraceLevel)NEARBY_TRACE_LEVEL_##level, \
                       __VA_ARGS__)
#ifdef __cplusplus
}
#endif

#endif /* NEARBY_TRACE_H */
