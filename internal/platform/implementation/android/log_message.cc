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

#include "internal/platform/implementation/android/log_message.h"

#include <cstdarg>
#include <cstdio>
#include <ostream>
#include <string>
#include "absl/base/log_severity.h"
#include "internal/platform/implementation/log_message.h"

namespace nearby {
namespace android {

namespace {

// This is a partial copy of base::StringAppendV for OSS compilation.
void NearbyStringAppendV(std::string* dst, const char* format, va_list ap) {
  // Fixed size buffer 1024 should be big enough.
  static const int kSpaceLength = 1024;
  char space[kSpaceLength];
  int result = vsnprintf(space, kSpaceLength, format, ap);
  va_end(ap);
  dst->append(space, result);
}
}  // namespace

api::LogMessage::Severity g_min_log_severity = api::LogMessage::Severity::kInfo;

inline absl::LogSeverity ConvertSeverity(api::LogMessage::Severity severity) {
  switch (severity) {
    // api::LogMessage::Severity kVerbose and kInfo is mapped to
    // absl::LogSeverity kInfo since absl::LogSeverity doesn't have kVerbose
    // level.
    case api::LogMessage::Severity::kVerbose:
    case api::LogMessage::Severity::kInfo:
      return absl::LogSeverity::kInfo;
    case api::LogMessage::Severity::kWarning:
      return absl::LogSeverity::kWarning;
    case api::LogMessage::Severity::kError:
      return absl::LogSeverity::kError;
    case api::LogMessage::Severity::kFatal:
      return absl::LogSeverity::kFatal;
  }
}

LogMessage::LogMessage(const char* file, int line, Severity severity)
    : log_streamer_(ConvertSeverity(severity), file, line) {}

LogMessage::~LogMessage() = default;

void LogMessage::Print(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::string result;
  NearbyStringAppendV(&result, format, ap);
  log_streamer_.stream() << result;
  va_end(ap);
}

std::ostream& LogMessage::Stream() { return log_streamer_.stream(); }

}  // namespace android

namespace api {

void LogMessage::SetMinLogSeverity(Severity severity) {
  android::g_min_log_severity = severity;
}

bool LogMessage::ShouldCreateLogMessage(Severity severity) {
  return severity >= android::g_min_log_severity;
}

}  // namespace api
}  // namespace nearby
