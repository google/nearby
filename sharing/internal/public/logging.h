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
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "absl/log/vlog_is_on.h"
// IWYU pragma: end_exports
#endif  // defined(NEARBY_CHROMIUM)

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_LOGGING_H_
