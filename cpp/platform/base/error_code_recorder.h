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

#ifndef PLATFORM_BASE_ERROR_CODE_RECORDER_H_
#define PLATFORM_BASE_ERROR_CODE_RECORDER_H_

#include <functional>

#include "platform/base/error_code_params.h"

namespace location {
namespace nearby {

// Deploys the error code of the platform medium to the analytics recorder.
//
// The usage is to call static method ErrorCode::LogErrorCode(...) in each
// medium platform supported whenever error occurred.
class ErrorCodeRecorder {
 public:
  using ErrorCodeListener = std::function<void(const ErrorCodeParams&)>;

  explicit ErrorCodeRecorder(ErrorCodeListener listener) {
    listener_ = std::move(listener);
  }
  ErrorCodeRecorder(const ErrorCodeRecorder&) = delete;
  ErrorCodeRecorder& operator=(const ErrorCodeRecorder&) = delete;
  virtual ~ErrorCodeRecorder() = default;

  static void LogErrorCode(
      location::nearby::proto::connections::Medium medium,
      location::nearby::errorcode::proto::Event event, int error,
      location::nearby::errorcode::proto::Description description,
      const std::string& pii_message, const std::string& connection_token);

 private:
  static ErrorCodeParams BuildErrorCodeParams(
      location::nearby::proto::connections::Medium medium,
      location::nearby::errorcode::proto::Event event, int error,
      location::nearby::errorcode::proto::Description description,
      const std::string& pii_message, const std::string& connection_token);

  // A listener to call back to AnlayticsRecorder.OnErrorCode() by building
  // error_code_params.
  static ErrorCodeListener listener_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_ERROR_CODE_RECORDER_H_
