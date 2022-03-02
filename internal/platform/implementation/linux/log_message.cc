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

#include "internal/platform/implementation/linux/log_message.h"
#include <absl/base/log_severity.h>
#include <algorithm>
#include <cstdio>
#include <stdarg.h>
namespace location {
namespace nearby {
namespace linux {

namespace {

// This is a partial copy of base::StringAppendV for OSS compilation.
void NearbyStringAppendV(std::string *dst, const char *format, va_list ap) {
  // Fixed size buffer 1024 should be big enough.
  static const int kSpaceLength = 1024;
  char space[kSpaceLength];
  int result = vsnprintf(space, kSpaceLength, format, ap);
  va_end(ap);
  dst->append(space, result);
}
} // namespace

api::LogMessage::Severity g_min_log_severity = api::LogMessage::Severity::kInfo;
inline google::LogSeverity ConvertSeverity(api::LogMessage::Severity severity) {
  switch (severity) {
  // api::LogMessage::Severity kVerbose and kInfo is mapped to
  // absl::LogSeverity kInfo since absl::LogSeverity doesn't have kVerbose
  // level.
  case api::LogMessage::Severity::kVerbose:
  case api::LogMessage::Severity::kInfo:
    return google::GLOG_INFO;
  case api::LogMessage::Severity::kWarning:
    return google::GLOG_WARNING;
  case api::LogMessage::Severity::kError:
    return google::GLOG_ERROR;
  case api::LogMessage::Severity::kFatal:
    return google::GLOG_FATAL;
  }
}

LogMessage::LogMessage(const char *file, int line, Severity severity)
    : log_streamer_(file, line, ConvertSeverity(severity)) {}

LogMessage::~LogMessage() = default;

void LogMessage::Print(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  std::string result;
  NearbyStringAppendV(&result, format, ap);
  log_streamer_.stream() << result;
  va_end(ap);
}

std::ostream &LogMessage::Stream() { return log_streamer_.stream(); }

} // namespace linux

namespace api {

void LogMessage::SetMinLogSeverity(Severity severity) {
  linux::g_min_log_severity = severity;
}

bool LogMessage::ShouldCreateLogMessage(Severity severity) {
  return severity >= linux::g_min_log_severity;
}

} // namespace api
} // namespace nearby
} // namespace location
