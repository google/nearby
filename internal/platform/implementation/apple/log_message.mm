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

#include "internal/platform/implementation/apple/log_message.h"

#include <ostream>
#include <string>

#include "internal/platform/implementation/log_message.h"

namespace nearby {
namespace apple {

api::LogMessage::Severity gMinLogSeverity = api::LogMessage::Severity::kInfo;

GTMLoggerLevel ConvertSeverity(api::LogMessage::Severity severity) {
  switch (severity) {
    case api::LogMessage::Severity::kVerbose:
      return kGTMLoggerLevelDebug;
    case api::LogMessage::Severity::kInfo:
      return kGTMLoggerLevelInfo;
    case api::LogMessage::Severity::kWarning:
      return kGTMLoggerLevelInfo;
    case api::LogMessage::Severity::kError:
      return kGTMLoggerLevelError;
    case api::LogMessage::Severity::kFatal:
      return kGTMLoggerLevelAssert;
  }
}

// GTMLogger expects a function name, but we only have file and line. So format the info as
// {basename(file)}:{line} and use that as the function name.
std::string ConvertFileAndLine(absl::string_view filepath, int line) {
  size_t path = filepath.find_last_of('/');
  if (path != filepath.npos) filepath.remove_prefix(path + 1);
  return std::string(filepath) + ":" + std::to_string(line);
}

LogStreamer::LogStreamer(GTMLoggerLevel severity, absl::string_view func)
    : severity_(severity), func_(func) {}

LogStreamer::~LogStreamer() {
  switch (severity_) {
    case kGTMLoggerLevelDebug:
      [[GTMLogger sharedLogger] logFuncDebug:func_.c_str() msg:@"%@", @(stream_.str().c_str())];
      break;
    case kGTMLoggerLevelInfo:
      [[GTMLogger sharedLogger] logFuncInfo:func_.c_str() msg:@"%@", @(stream_.str().c_str())];
      break;
    case kGTMLoggerLevelError:
      [[GTMLogger sharedLogger] logFuncError:func_.c_str() msg:@"%@", @(stream_.str().c_str())];
      break;
    case kGTMLoggerLevelAssert:
      [[GTMLogger sharedLogger] logFuncAssert:func_.c_str() msg:@"%@", @(stream_.str().c_str())];
      break;
    case kGTMLoggerLevelUnknown:
      // no-op
      break;
  }
}

LogMessage::LogMessage(const char* file, int line, Severity severity)
    : log_streamer_(ConvertSeverity(severity), ConvertFileAndLine(file, line)),
      severity_(ConvertSeverity(severity)),
      func_(ConvertFileAndLine(file, line)) {}

void LogMessage::Print(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  NSString *msg = [[NSString alloc] initWithFormat:@(format) arguments:ap];
  switch (severity_) {
    case kGTMLoggerLevelDebug:
      [[GTMLogger sharedLogger] logFuncDebug:func_.c_str() msg:@"%@", msg];
      break;
    case kGTMLoggerLevelInfo:
      [[GTMLogger sharedLogger] logFuncInfo:func_.c_str() msg:@"%@", msg];
      break;
    case kGTMLoggerLevelError:
      [[GTMLogger sharedLogger] logFuncError:func_.c_str() msg:@"%@", msg];
      break;
    case kGTMLoggerLevelAssert:
      [[GTMLogger sharedLogger] logFuncAssert:func_.c_str() msg:@"%@", msg];
      break;
    case kGTMLoggerLevelUnknown:
      // no-op
      break;
  }
  va_end(ap);
}

std::ostream& LogMessage::Stream() { return log_streamer_.stream(); }

}  // namespace apple

namespace api {

// static
void LogMessage::SetMinLogSeverity(Severity severity) { apple::gMinLogSeverity = severity; }

// static
bool LogMessage::ShouldCreateLogMessage(Severity severity) {
  return severity >= apple::gMinLogSeverity;
}

}  // namespace api
}  // namespace nearby
