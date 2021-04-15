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

#ifndef PLATFORM_IMPL_WINDOWS_LOG_MESSAGE_H_
#define PLATFORM_IMPL_WINDOWS_LOG_MESSAGE_H_

#include "platform/api/log_message.h"

namespace location {
namespace nearby {
namespace windows {

// A log message that prints to appropraite destination when ~LogMessage() is
// called.
//
// note: the Severity enum should map (best effort) to the corresponding level
// id that the platform logging implementation has.
class LogMessage : public api::LogMessage {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~LogMessage() override = default;

  // Printf like logging.
  // TODO(b/184975123): replace with real implementation.
  void Print(const char* format, ...) override {}

  // Returns a stream for std::cout like logging.
  // TODO(b/184975123): replace with real implementation.
  std::ostream& Stream() override { return empty_stream_; }

  // TODO(b/184975123): replace with real implementation.
  std::ostream empty_stream_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_LOG_MESSAGE_H_
