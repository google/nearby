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

#ifndef PLATFORM_BASE_LOGGING_H_
#define PLATFORM_BASE_LOGGING_H_

// base/logging.h is only included to allow logging clients to include CHECK's.
// In Chrome this is base/check.h. See crbug/1212611.
#ifdef NEARBY_CHROMIUM
#include "base/check.h"
// base/logging.h is available externally as "glog". However, this repo contains
// template files that can't be built by Swift Package Manager. To build with
// SPM we only need CHECK and DCHECK defined.
#elif defined(NEARBY_SWIFTPM)
#include <sstream>
#define CHECK(condition) static_cast<void>(0), condition ? (void) 0 : abort()
#define DCHECK(condition) static_cast<void>(0), (void) 0
#define CHECK_GT(a, b) static_cast<void>(0), a > b ? (void) 0 : abort()
#define DCHECK_GT(a, b) static_cast<void>(0), a > b ? (void) 0 : abort()
#define CHECK_LE(a, b) static_cast<void>(0), a <= b ? (void) 0 : abort()
#define DCHECK_LE(a, b) static_cast<void>(0), a <= b ? (void) 0 : abort()
#define CHECK_NE(a, b) static_cast<void>(0), a != b ? (void) 0 : abort()
#define DCHECK_NE(a, b) static_cast<void>(0), a != b ? (void) 0 : abort()
#define CHECK_EQ(a, b) static_cast<void>(0), a == b ? (void) 0 : abort()
#define DCHECK_EQ(a, b) static_cast<void>(0), a == b ? (void) 0 : abort()
#define CHECK_GE(a, b) static_cast<void>(0), a >= b ? (void) 0 : abort()
#define DCHECK_GE(a, b) static_cast<void>(0), a >= b ? (void) 0 : abort()
#else
#include "glog/logging.h"
#endif
#include "internal/platform/implementation/log_message.h"
#include "internal/platform/implementation/platform.h"

namespace nearby {

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class LogMessageVoidify {
 public:
  LogMessageVoidify() = default;
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) {}
};

}  // namespace nearby

// Severity enum conversion
#define NEARBY_SEVERITY_VERBOSE nearby::api::LogMessage::Severity::kVerbose
#define NEARBY_SEVERITY_INFO nearby::api::LogMessage::Severity::kInfo
#define NEARBY_SEVERITY_WARNING nearby::api::LogMessage::Severity::kWarning
#define NEARBY_SEVERITY_ERROR nearby::api::LogMessage::Severity::kError
#define NEARBY_SEVERITY_FATAL nearby::api::LogMessage::Severity::kFatal
#if defined(_WIN32)
// wingdi.h defines ERROR to be 0. When we call LOG(ERROR), it gets substituted
// with 0, and it expands to NEARBY_SEVERITY_0. To allow us to keep using this
// syntax, we define this macro to do the same thing as NEARBY_SEVERITY_ERROR.
#define NEARBY_SEVERITY_0 nearby::api::LogMessage::Severity::kError
#endif  // defined(_WIN32)
#define NEARBY_SEVERITY(severity) NEARBY_SEVERITY_##severity

// Log enabling
#define NEARBY_LOG_IS_ON(severity) \
  nearby::api::LogMessage::ShouldCreateLogMessage(NEARBY_SEVERITY(severity))

#define NEARBY_LOG_SET_SEVERITY(severity) \
  nearby::api::LogMessage::SetMinLogSeverity(NEARBY_SEVERITY(severity))

// Log message creation
#define NEARBY_LOG_MESSAGE(severity)                     \
  nearby::api::ImplementationPlatform::CreateLogMessage( \
      __FILE__, __LINE__, NEARBY_SEVERITY(severity))

// Public APIs
// The stream statement must come last or otherwise it won't compile.
#define NEARBY_LOGS(severity)   \
  !(NEARBY_LOG_IS_ON(severity)) \
      ? (void)0                 \
      : nearby::LogMessageVoidify() & NEARBY_LOG_MESSAGE(severity)->Stream()

#define NEARBY_LOG(severity, ...) \
  NEARBY_LOG_IS_ON(severity)      \
  ? NEARBY_LOG_MESSAGE(severity)->Print(__VA_ARGS__) : (void)0

#ifdef NEARBY_SWIFTPM
#define LOG(severity) NEARBY_LOGS(severity)
#endif

#endif  // PLATFORM_BASE_LOGGING_H_
