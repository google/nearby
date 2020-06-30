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

#ifndef PLATFORM_V2_BASE_LOGGING_H_
#define PLATFORM_V2_BASE_LOGGING_H_

#include "platform_v2/api/log_message.h"
#include "platform_v2/api/platform.h"

namespace location {
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
}  // namespace location

// Severity enum conversion
#define NEARBY_SEVERITY_INFO location::nearby::api::LogMessage::Severity::kInfo
#define NEARBY_SEVERITY_WARNING \
  location::nearby::api::LogMessage::Severity::kWarning
#define NEARBY_SEVERITY_ERROR \
  location::nearby::api::LogMessage::Severity::kError
#define NEARBY_SEVERITY_FATAL \
  location::nearby::api::LogMessage::Severity::kFatal
#if defined(_WIN32)
// wingdi.h defines ERROR to be 0. When we call LOG(ERROR), it gets substituted
// with 0, and it expands to NEARBY_SEVERITY_0. To allow us to keep using this
// syntax, we define this macro to do the same thing as NEARBY_SEVERITY_ERROR.
#define NEARBY_SEVERITY_0 location::nearby::api::LogMessage::Severity::kError
#endif  // defined(_WIN32)
#define NEARBY_SEVERITY(severity) NEARBY_SEVERITY_##severity

// Log enabling
#define NEARBY_LOG_IS_ON(severity)                           \
  location::nearby::api::LogMessage::ShouldCreateLogMessage( \
      NEARBY_SEVERITY(severity))

#define NEARBY_LOG_SET_SEVERITY(severity)               \
  location::nearby::api::LogMessage::SetMinLogSeverity( \
      NEARBY_SEVERITY(severity))

// Log message creation
#define NEARBY_LOG_MESSAGE(severity)                               \
  location::nearby::api::ImplementationPlatform::CreateLogMessage( \
      __FILE__, __LINE__, NEARBY_SEVERITY(severity))

// Public APIs
// The stream statement must come last or otherwise it won't compile.
#define NEARBY_LOGS(severity)                                             \
  !(NEARBY_LOG_IS_ON(severity)) ? (void)0                                 \
                                : location::nearby::LogMessageVoidify() & \
                                      NEARBY_LOG_MESSAGE(severity)->Stream()

#define NEARBY_LOG(severity, ...) \
  NEARBY_LOG_IS_ON(severity)      \
  ? NEARBY_LOG_MESSAGE(severity)->Print(__VA_ARGS__) : (void)0

#endif  // PLATFORM_V2_BASE_LOGGING_H_
