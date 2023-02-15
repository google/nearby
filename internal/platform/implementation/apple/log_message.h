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

#ifndef PLATFORM_IMPL_APPLE_LOG_MESSAGE_H_
#define PLATFORM_IMPL_APPLE_LOG_MESSAGE_H_

#include <ostream>
#include <sstream>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/log_message.h"
#include "GoogleToolboxForMac/GTMLogger.h"

namespace nearby {
namespace apple {

class LogStreamer final {
 public:
  explicit LogStreamer(GTMLoggerLevel severity, absl::string_view func);

  ~LogStreamer();

  std::ostream& stream() { return stream_; }

 private:
  GTMLoggerLevel severity_;
  std::string func_;
  std::ostringstream stream_;
};

// Concrete LogMessage implementation
class LogMessage : public api::LogMessage {
 public:
  LogMessage(const char* file, int line, Severity severity);
  ~LogMessage() override = default;

  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;

  void Print(const char* format, ...) override;

  std::ostream& Stream() override;

 private:
  LogStreamer log_streamer_;
  GTMLoggerLevel severity_;
  std::string func_;
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_LOG_MESSAGE_H_
