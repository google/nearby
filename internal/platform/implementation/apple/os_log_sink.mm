// Copyright 2025 Google LLC
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

#include "internal/platform/implementation/apple/os_log_sink.h"

#include <os/log.h>

#include "absl/log/log_entry.h"
#include "absl/strings/str_cat.h"

namespace nearby {
namespace apple {

OsLogSink::OsLogSink(const std::string& subsystem)
    : log_(os_log_create(subsystem.c_str(), "default")) {}

void OsLogSink::Send(const absl::LogEntry& entry) {
  os_log_type_t type = OS_LOG_TYPE_DEFAULT;
  // Severity tag added to the message text, since INFO and WARNING share the same OSLog type.
  const char* severity_tag = "I";
  switch (entry.log_severity()) {
    case absl::LogSeverity::kInfo:
      // Map INFO to DEFAULT rather than OS_LOG_TYPE_INFO. INFO-type messages are kept in memory
      // only, so they are missing from sysdiagnose / `log collect` archives and hidden in
      // Console.app by default, which makes field bugs hard to debug. VLOG still maps to DEBUG.
      type = OS_LOG_TYPE_DEFAULT;
      severity_tag = "I";
      break;
    case absl::LogSeverity::kWarning:
      type = OS_LOG_TYPE_DEFAULT;  // OSLog doesn't have a distinct 'Warning' level
      severity_tag = "W";
      break;
    case absl::LogSeverity::kError:
      type = OS_LOG_TYPE_ERROR;
      severity_tag = "E";
      break;
    case absl::LogSeverity::kFatal:
      type = OS_LOG_TYPE_FAULT;
      severity_tag = "F";
      break;
  }

  // Map all VLOG levels (VLOG(1), VLOG(2), ...) to DEBUG. Non-VLOG entries have a verbosity of
  // absl::LogEntry::kNoVerbosityLevel (-1).
  if (entry.verbosity() != absl::LogEntry::kNoVerbosityLevel) {
    type = OS_LOG_TYPE_DEBUG;
    severity_tag = "V";
  }

  // Format and send the log message to OSLog, e.g. "[W][ble.cc:123] message".
  std::string message = absl::StrCat("[", severity_tag, "][", entry.source_basename(), ":",
                                     entry.source_line(), "] ", entry.text_message());

  os_log_with_type(log_, type, "%{public}s", message.c_str());
}

}  // namespace apple
}  // namespace nearby
