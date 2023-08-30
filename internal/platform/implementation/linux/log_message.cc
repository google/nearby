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
static std::unique_ptr<linux::LogControl> global_log_control_;
static absl::once_flag log_control_init_;

static void init_log_control(std::nullptr_t) {
  global_log_control_ =
      std::make_unique<linux::LogControl>(linux::getDefaultBusConnection());
}

namespace api {
void LogMessage::SetMinLogSeverity(Severity severity) {
  absl::call_once(log_control_init_, init_log_control, nullptr);
  assert(global_log_control_ != nullptr);
  global_log_control_->LogLevel(severity);
}

bool LogMessage::ShouldCreateLogMessage(Severity severity) {
  absl::call_once(log_control_init_, init_log_control, nullptr);
  assert(global_log_control_ != nullptr);
  return severity >= global_log_control_->GetLogLevel();
}

}  // namespace api
namespace linux {
static inline google::LogSeverity ConvertSeverity(
    api::LogMessage::Severity severity) {
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
static inline int ConvertSeverityToSyslog(google::LogSeverity severity) {
  switch (severity) {
    case google::GLOG_WARNING:
      return LOG_WARNING;
    case google::GLOG_ERROR:
      return LOG_ERR;
    case google::GLOG_FATAL:
      return LOG_EMERG;
    case google::GLOG_INFO:
    default:
      return LOG_INFO;
  }
}

// TODO: Set a LogSink depending on the target set by LogControl
LogMessage::LogMessage(const char *file, int line, Severity severity)
    : log_streamer_(file, line, ConvertSeverity(severity),
                    global_log_control_.get(), false) {}

static absl::Mutex cout_mutex;

void LogControl::send(google::LogSeverity severity, const char *full_filename,
                      const char *base_filename, int line,
                      const struct ::tm *tm_time, const char *message,
                      size_t message_len) {
  switch (log_target_) {
    case kJournal:
      sd_journal_send("MESSAGE=%s", message, "PRIORITY=%d",
                      ConvertSeverityToSyslog(severity), "CODE_FILE=%s",
                      base_filename, "CODE_LINE=%d", line, NULL);
      break;
    case kSyslog: {
      auto str = LogSink::ToString(severity, base_filename, line, tm_time,
                                   message, message_len);
      syslog(ConvertSeverityToSyslog(severity), "%s", str.c_str());
      break;
    }
    case kConsole:
    default:
      absl::MutexLock l(&cout_mutex);
      std::cout << LogSink::ToString(severity, base_filename, line, tm_time,
                                     message, message_len)
                << "\n";
      break;
  }
}

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
}  // namespace nearby
