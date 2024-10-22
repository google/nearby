// Copyright 2024 Google LLC
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

#ifndef PLATFORM_BASE_LOGGING_H_
#define PLATFORM_BASE_LOGGING_H_

#if defined(NEARBY_CHROMIUM)
// Chromium does not use absl log.  Forward to Chromium native log macros.
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#else                                    // defined(NEARBY_CHROMIUM)
// IWYU pragma: begin_exports
#include "absl/log/check.h"  // nogncheck
#include "absl/log/globals.h"  // nogncheck
#include "absl/log/log.h"    // nogncheck
// IWYU pragma: end_exports
#endif                                   // defined(NEARBY_CHROMIUM)

// Public APIs
// The stream statement must come last, or it won't compile.
#define NEARBY_VLOG(level) VLOG(level)
#define NEARBY_LOGS(severity) LOG(severity)

#define NEARBY_DLOG(severity) DLOG(severity)
#define NEARBY_DVLOG(severity) DVLOG(severity)

#endif  // PLATFORM_BASE_LOGGING_H_
