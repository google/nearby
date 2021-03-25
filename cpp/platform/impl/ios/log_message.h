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

#ifndef PLATFORM_IMPL_IOS_LOG_MESSAGE_H_
#define PLATFORM_IMPL_IOS_LOG_MESSAGE_H_

#include "base/logging.h"
#include "platform/api/log_message.h"

namespace location {
namespace nearby {
namespace ios {

class LogMessage : public api::LogMessage {
 public:
  LogMessage(const char* file, int line, Severity severity);
  ~LogMessage() override;

  std::ostream& Stream() override;

 private:
  absl::LogStreamer log_streamer_;
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_IOS_LOG_MESSAGE_H_
