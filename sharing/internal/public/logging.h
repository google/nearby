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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_LOGGING_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_LOGGING_H_

#if defined(NEARBY_CHROMIUM)
// Chromium does not use absl log.  Forward to Chromium native log macros.
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#else  // defined(NEARBY_CHROMIUM)
// IWYU pragma: begin_exports
#include "absl/log/check.h"
#include "absl/log/log.h"
// IWYU pragma: end_exports
#endif  // defined(NEARBY_CHROMIUM)

// Public APIs
// The stream statement must come last, or it won't compile.
#define NL_VLOG(level) VLOG(level)
#define NL_LOG(severity) LOG(severity)

#define NL_DLOG(severity) DLOG(severity)
#define NL_DVLOG(severity) DVLOG(severity)

#define NL_CHECK(expr) CHECK(expr)
#define NL_CHECK_EQ(a, b) CHECK_EQ((a), (b))
#define NL_CHECK_NE(a, b) CHECK_NE((a), (b))
#define NL_CHECK_GE(a, b) CHECK_GE((a), (b))
#define NL_CHECK_GT(a, b) CHECK_GT((a), (b))
#define NL_CHECK_LE(a, b) CHECK_LE((a), (b))
#define NL_CHECK_LT(a, b) CHECK_LT((a), (b))

#define NL_DCHECK(expr) DCHECK((expr))
#define NL_DCHECK_EQ(a, b) DCHECK_EQ((a), (b))
#define NL_DCHECK_NE(a, b) DCHECK_NE((a), (b))
#define NL_DCHECK_GE(a, b) DCHECK_GE((a), (b))
#define NL_DCHECK_GT(a, b) DCHECK_GT((a), (b))
#define NL_DCHECK_LE(a, b) DCHECK_LE((a), (b))
#define NL_DCHECK_LT(a, b) DCHECK_LT((a), (b))

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_LOGGING_H_
