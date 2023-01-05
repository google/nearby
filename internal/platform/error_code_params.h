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

#ifndef PLATFORM_BASE_ERROR_CORE_PARAMS_H_
#define PLATFORM_BASE_ERROR_CORE_PARAMS_H_

#include "proto/connections_enums.pb.h"
#include "proto/errorcode/error_code_enums.pb.h"

namespace nearby {

// A struct to construct error code parameters for the analytics recorder.
struct ErrorCodeParams {
  location::nearby::proto::connections::Medium medium =
      location::nearby::proto::connections::UNKNOWN_MEDIUM;
  location::nearby::errorcode::proto::Event event =
      location::nearby::errorcode::proto::UNKNOWN_EVENT;
  location::nearby::errorcode::proto::Description description =
      location::nearby::errorcode::proto::UNKNOWN;
  std::string pii_message = {};
  bool is_common_error = false;
  location::nearby::errorcode::proto::CommonError common_error =
      location::nearby::errorcode::proto::UNKNOWN_ERROR;
  location::nearby::errorcode::proto::ConnectError connect_error =
      location::nearby::errorcode::proto::UNKNOWN_CONNECT_ERROR;
  location::nearby::errorcode::proto::DisconnectError disconnect_error =
      location::nearby::errorcode::proto::UNKNOWN_DISCONNECT_ERROR;
  location::nearby::errorcode::proto::StartAdvertisingError
      start_advertising_error =
          location::nearby::errorcode::proto::UNKNOWN_START_ADVERTISING_ERROR;
  location::nearby::errorcode::proto::StopAdvertisingError
      stop_advertising_error =
          location::nearby::errorcode::proto::UNKNOWN_STOP_ADVERTISING_ERROR;
  location::nearby::errorcode::proto::StartDiscoveringError
      start_discovering_error =
          location::nearby::errorcode::proto::UNKNOWN_START_DISCOVERING_ERROR;
  location::nearby::errorcode::proto::StopDiscoveringError
      stop_discovering_error =
          location::nearby::errorcode::proto::UNKNOWN_STOP_DISCOVERING_ERROR;
  location::nearby::errorcode::proto::StartListeningIncomingConnectionError
      start_listening_incoming_connection_error = location::nearby::errorcode::
          proto::UNKNOWN_START_LISTENING_INCOMING_CONNECTION_ERROR;
  location::nearby::errorcode::proto::StopListeningIncomingConnectionError
      stop_listening_incoming_connection_error = location::nearby::errorcode::
          proto::UNKNOWN_STOP_LISTENING_INCOMING_CONNECTION_ERROR;
  std::string connection_token = {};
};

}  // namespace nearby

#endif  // PLATFORM_BASE_ERROR_CORE_PARAMS_H_
