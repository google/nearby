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

#include "absl/functional/any_invocable.h"
#include "internal/platform/error_code_params.h"

namespace nearby {

// Deploys the error code of the platform medium to the analytics recorder.
//
// The usage is to call static method ErrorCode::LogErrorCode(...) in each
// medium platform supported whenever error occurred.
class ErrorCodeRecorder {
 public:
  using ErrorCodeListener = absl::AnyInvocable<void(const ErrorCodeParams&)>;

  explicit ErrorCodeRecorder(ErrorCodeListener listener) {
    listener_ = std::move(listener);
  }
  ErrorCodeRecorder(const ErrorCodeRecorder&) = delete;
  ErrorCodeRecorder& operator=(const ErrorCodeRecorder&) = delete;

  // Logs error code when medium error occurred.
  //
  // Caller should pass the exact matched event and error enum value except
  // the common error.
  // e.g.
  //   - START_ADVERTISING_FAILED(43) in StartAdvertisingError =>
  //     START_ADVERTISING event.
  //   - DISCONNECT_NETWORK_FAILED(31) in Disconnect_error => DISCONNECT event.
  //   - INVALID_PARAMETER(1) for Common_error, and the event will be kept as it
  //     is passed in.
  //
  // medium           - An enum defined in proto::connections::Medium.
  // event            - An enum defined in errorcode::proto::Event.
  // error            - An enum integer value in the range from 0..30 defined
  //                    in errorcode::proto::CommonError and the range from 31..
  //                    nn defined among
  //                    errorcode::proto::StartAdvertisingError to
  //                    errorcode::proto::DisconnectError.
  // description      - A description defiend in errorcode::proto::description.
  // pii_message      - A pii info defiend in errorcode::proto::pii_message. An
  //                    empty string won't be recorded.
  // connection_token - connection token string.
  static void LogErrorCode(
      location::nearby::proto::connections::Medium medium,
      location::nearby::errorcode::proto::Event event, int error,
      location::nearby::errorcode::proto::Description description,
      const std::string& pii_message, const std::string& connection_token);

  // An auxiliary funciton for LogError() to assemble the ErrorCodeParams
  // struct.
  //
  // See `LogErrorCode` for reference on the parameters.
  static ErrorCodeParams BuildErrorCodeParams(
      location::nearby::proto::connections::Medium medium,
      location::nearby::errorcode::proto::Event event, int error,
      location::nearby::errorcode::proto::Description description,
      const std::string& pii_message, const std::string& connection_token);

 private:
  // A listener to call back to AnlayticsRecorder.OnErrorCode() by building
  // error_code_params.
  static ErrorCodeListener listener_;
};

}  // namespace nearby

#endif  // PLATFORM_BASE_ERROR_CODE_RECORDER_H_
