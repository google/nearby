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

#include "internal/platform/error_code_recorder.h"

#include "internal/platform/logging.h"
#include "proto/errorcode/error_code_enums.pb.h"

namespace nearby {

using ::location::nearby::errorcode::proto::CommonError;
using ::location::nearby::errorcode::proto::CommonError_IsValid;
using ::location::nearby::errorcode::proto::CONNECT;
using ::location::nearby::errorcode::proto::ConnectError;
using ::location::nearby::errorcode::proto::Description;
using ::location::nearby::errorcode::proto::DISCONNECT;
using ::location::nearby::errorcode::proto::DisconnectError;
using ::location::nearby::errorcode::proto::Event;
using ::location::nearby::errorcode::proto::START_ADVERTISING;
using ::location::nearby::errorcode::proto::START_DISCOVERING;
using ::location::nearby::errorcode::proto::START_LISTENING_INCOMING_CONNECTION;
using ::location::nearby::errorcode::proto::StartAdvertisingError;
using ::location::nearby::errorcode::proto::StartDiscoveringError;
using ::location::nearby::errorcode::proto::
    StartListeningIncomingConnectionError;
using ::location::nearby::errorcode::proto::STOP_ADVERTISING;
using ::location::nearby::errorcode::proto::STOP_DISCOVERING;
using ::location::nearby::errorcode::proto::STOP_LISTENING_INCOMING_CONNECTION;
using ::location::nearby::errorcode::proto::StopAdvertisingError;
using ::location::nearby::errorcode::proto::StopDiscoveringError;
using ::location::nearby::errorcode::proto::
    StopListeningIncomingConnectionError;
using ::location::nearby::errorcode::proto::UNKNOWN_ERROR;
using ::location::nearby::proto::connections::Medium;

// Default static no-op listener
ErrorCodeRecorder::ErrorCodeListener ErrorCodeRecorder::listener_ =
    [](const ErrorCodeParams&) {};

void ErrorCodeRecorder::LogErrorCode(Medium medium, Event event, int error,
                                     Description description,
                                     const std::string& pii_message,
                                     const std::string& connection_token) {
  NEARBY_LOGS(INFO) << "ErrorCodeRecorder LogErrorCode";
  ErrorCodeParams params = BuildErrorCodeParams(
      medium, event, error, description, pii_message, connection_token);
  listener_(params);
}

ErrorCodeParams ErrorCodeRecorder::BuildErrorCodeParams(
    Medium medium, Event event, int error, Description description,
    const std::string& pii_message, const std::string& connection_token) {
  ErrorCodeParams params = {.medium = medium,
                            .event = event,
                            .description = description,
                            .pii_message = pii_message,
                            .connection_token = connection_token};

  if (CommonError_IsValid(error)) {
    params.common_error = static_cast<CommonError>(error);
    params.is_common_error = true;
  } else {
    params.is_common_error = false;
    switch (event) {
      case START_ADVERTISING:
        params.start_advertising_error =
            static_cast<StartAdvertisingError>(error);
        break;
      case STOP_ADVERTISING:
        params.stop_advertising_error =
            static_cast<StopAdvertisingError>(error);
        break;
      case START_LISTENING_INCOMING_CONNECTION:
        params.start_listening_incoming_connection_error =
            static_cast<StartListeningIncomingConnectionError>(error);
        break;
      case STOP_LISTENING_INCOMING_CONNECTION:
        params.stop_listening_incoming_connection_error =
            static_cast<StopListeningIncomingConnectionError>(error);
        break;
      case START_DISCOVERING:
        params.start_discovering_error =
            static_cast<StartDiscoveringError>(error);
        break;
      case STOP_DISCOVERING:
        params.stop_discovering_error =
            static_cast<StopDiscoveringError>(error);
        break;
      case CONNECT:
        params.connect_error = static_cast<ConnectError>(error);
        break;
      case DISCONNECT:
        params.disconnect_error = static_cast<DisconnectError>(error);
        break;
      // Set the error as unknown if undefined event passed in.
      default:
        params.common_error = UNKNOWN_ERROR;
        params.is_common_error = true;
        break;
    }
  }
  return params;
}

}  // namespace nearby
