#include "platform/impl/ios/log_message.h"

#include <algorithm>

#include "base/stringprintf.h"

namespace location {
namespace nearby {
namespace ios {

api::LogMessage::Severity kMinLogSeverity = api::LogMessage::Severity::kInfo;

inline absl::LogSeverity ConvertSeverity(api::LogMessage::Severity severity) {
  switch (severity) {
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
  StringAppendV(&result, format, ap);
  log_streamer_.stream() << result;
  va_end(ap);
}

std::ostream& LogMessage::Stream() { return log_streamer_.stream(); }

}  // namespace ios

namespace api {

void LogMessage::SetMinLogSeverity(Severity severity) {
  ios::kMinLogSeverity = severity;
}

bool LogMessage::ShouldCreateLogMessage(Severity severity) {
  return severity >= ios::kMinLogSeverity;
}

}  // namespace api
}  // namespace nearby
}  // namespace location
