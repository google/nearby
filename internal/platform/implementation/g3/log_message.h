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

#ifndef PLATFORM_IMPL_G3_LOG_MESSAGE_H_
#define PLATFORM_IMPL_G3_LOG_MESSAGE_H_

#include "glog/logging.h"
#include "internal/platform/implementation/log_message.h"

namespace nearby {
namespace g3 {

// See documentation in
// cpp/platform/api/log_message.h
class LogMessage : public api::LogMessage {
 public:
  LogMessage(const char* file, int line, Severity severity);
  ~LogMessage() override;

  void Print(const char* format, ...) override;

  std::ostream& Stream() override;

 private:
  google::LogMessage log_streamer_;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_LOG_MESSAGE_H_
