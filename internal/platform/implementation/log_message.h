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

#ifndef PLATFORM_API_LOG_MESSAGE_H_
#define PLATFORM_API_LOG_MESSAGE_H_

#include <iostream>

namespace nearby {
namespace api {

// A log message that prints to appropraite destination when ~LogMessage() is
// called.
//
// note: the Severity enum should map (best effort) to the corresponding level
// id that the platform logging implementation has.
class LogMessage {
 public:
  enum class Severity {
    kVerbose = -1,
    kInfo = 0,
    kWarning = 1,
    kError = 2,
    kFatal = 3,  // Terminates the process after logging
  };

  // Configures minimum severity to be logged.
  static void SetMinLogSeverity(Severity severity);

  // Returns if a log with |severity| should be logged based on
  // SetMinLogSeverity and additional platform requirements.
  static bool ShouldCreateLogMessage(Severity severity);

  virtual ~LogMessage() = default;

  // Printf like logging.
  virtual void Print(const char* format, ...) = 0;

  // Returns a stream for std::cout like logging.
  virtual std::ostream& Stream() = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_LOG_MESSAGE_H_
