// Copyright 2023 Google LLC
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

#include <sys/syslog.h>
#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>

#define SD_JOURNAL_SUPPRESS_LOCATION true
#include <systemd/sd-journal.h>

#include "absl/base/call_once.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/log_message.h"

namespace nearby {
namespace linux {

std::atomic<api::LogMessage::Severity> min_log_severity_ =
    api::LogMessage::Severity::kInfo;

inline google::LogSeverity ConvertSeverity(api::LogMessage::Severity severity) {
  switch (severity) {
    case api::LogMessage::Severity::kWarning:
      return google::GLOG_WARNING;
    case api::LogMessage::Severity::kError:
      return google::GLOG_ERROR;
    case api::LogMessage::Severity::kFatal:
      return google::GLOG_FATAL;
    case api::LogMessage::Severity::kVerbose:
    case api::LogMessage::Severity::kInfo:
    default:
      return google::GLOG_INFO;
  }
}

LogMessage::LogMessage(const char *file, int line, Severity severity)
    : log_streamer_(file, line, ConvertSeverity(severity)) {}

void LogMessage::Print(const char *format, ...) {
  char *buf = nullptr;

  va_list ap;
  va_start(ap, format);
  auto ret = vasprintf(&buf, format, ap);
  if (ret > 0) {
    log_streamer_.stream() << std::string(buf, ret);
  }
  if (buf != nullptr) free(buf);
  va_end(ap);
}

std::ostream &LogMessage::Stream() { return log_streamer_.stream(); }

}  // namespace linux

namespace api {

void LogMessage::SetMinLogSeverity(Severity severity) {
  linux::min_log_severity_ = severity;
}

bool LogMessage::ShouldCreateLogMessage(Severity severity) {
  return severity >= linux::min_log_severity_;
}
}  // namespace api
}  // namespace nearby
