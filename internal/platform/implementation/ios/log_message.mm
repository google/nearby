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

#include "internal/platform/implementation/ios/log_message.h"

#ifndef NEARBY_SWIFTPM
#include "glog/logging.h"
#endif
#include "internal/platform/implementation/log_message.h"
#include "GoogleToolboxForMac/GTMLogger.h"

namespace location {
namespace nearby {
namespace ios {

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

LogMessage::LogMessage(const char* file, int line, Severity severity)
#ifdef NEARBY_SWIFTPM
    : log_streamer_() {}
#else
    : log_streamer_(ConvertSeverity(severity), file, line), severity_(severity) {}
#endif

void LogMessage::Print(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  switch (ConvertSeverity(severity_)) {
    case kGTMLoggerLevelDebug:
      [[GTMLogger sharedLogger] logDebug:[NSString stringWithUTF8String:format], ap];
      break;
    case kGTMLoggerLevelInfo:
      [[GTMLogger sharedLogger] logInfo:[NSString stringWithUTF8String:format], ap];
      break;
    case kGTMLoggerLevelError:
      [[GTMLogger sharedLogger] logError:[NSString stringWithUTF8String:format], ap];
      break;
    case kGTMLoggerLevelAssert:
      [[GTMLogger sharedLogger] logAssert:[NSString stringWithUTF8String:format], ap];
      break;
    case kGTMLoggerLevelUnknown:
        // no-op
        break;
  }
}

// TODO(b/169292092): GTMLogger doesn't support stream. Temporarily use absl LogStreamer to make
// build pass.
#ifdef NEARBY_SWIFTPM
std::ostream& LogMessage::Stream() { return log_streamer_; }
#else
std::ostream& LogMessage::Stream() { return log_streamer_.stream(); }
#endif

}  // namespace ios

namespace api {

// static
void LogMessage::SetMinLogSeverity(Severity severity) { ios::gMinLogSeverity = severity; }

// static
bool LogMessage::ShouldCreateLogMessage(Severity severity) {
  // TODO(b/169292092): GTMLogger doesn't support stream which cause crash. Temporarily turn off
  // LogMessage.
  return false;
}

}  // namespace api
}  // namespace nearby
}  // namespace location
