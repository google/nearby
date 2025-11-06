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

#ifndef PLATFORM_IMPL_APPLE_OS_LOG_SINK_H_
#define PLATFORM_IMPL_APPLE_OS_LOG_SINK_H_

#include <os/log.h>

#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"

namespace nearby {
namespace apple {

// OsLogSink is a LogSink that sends logs to the OS log.
class OsLogSink : public absl::LogSink {
 public:
  explicit OsLogSink(const std::string& subsystem);
  ~OsLogSink() override = default;

  // Sends the log entry to the OS log.
  void Send(const absl::LogEntry& entry) override;

 private:
  os_log_t log_;
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_OS_LOG_SINK_H_
