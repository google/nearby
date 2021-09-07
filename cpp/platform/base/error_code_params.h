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

#include "proto/connections_enums.proto.h"
#include "proto/errorcode/error_code_enums.proto.h"

namespace location {
namespace nearby {

// A struct to consturct error code parameters for the analytics recorder.
struct ErrorCodeParams {
  using Medium = ::location::nearby::proto::connections::Medium;
  using Event = ::location::nearby::errorcode::proto::Event;
  using Description = ::location::nearby::errorcode::proto::Description;
  using CommonError = ::location::nearby::errorcode::proto::CommonError;
  using ConnectError = ::location::nearby::errorcode::proto::ConnectError;
  using DisconnectError = ::location::nearby::errorcode::proto::DisconnectError;
  using StartAdvertisingError =
      ::location::nearby::errorcode::proto::StartAdvertisingError;
  using StopAdvertisingError =
      ::location::nearby::errorcode::proto::StopAdvertisingError;
  using StartDiscoveringError =
      ::location::nearby::errorcode::proto::StartDiscoveringError;
  using StopDiscoveringError =
      ::location::nearby::errorcode::proto::StopDiscoveringError;
  using StartListeningIncomingConnectionError = ::location::nearby::errorcode::
      proto::StartListeningIncomingConnectionError;
  using StopListeningIncomingConnectionError = ::location::nearby::errorcode::
      proto::StopListeningIncomingConnectionError;

  Medium medium = proto::connections::UNKNOWN_MEDIUM;
  Event event = errorcode::proto::UNKNOWN_EVENT;
  Description description = errorcode::proto::UNKNOWN;
  std::string pii_message = {};
  bool is_common_error = false;
  CommonError common_error = errorcode::proto::UNKNOWN_ERROR;
  ConnectError connect_error = errorcode::proto::UNKNOWN_CONNECT_ERROR;
  DisconnectError disconnect_error = errorcode::proto::UNKNOWN_DISCONNECT_ERROR;
  StartAdvertisingError start_advertising_error =
      errorcode::proto::UNKNOWN_START_ADVERTISING_ERROR;
  StopAdvertisingError stop_advertising_error =
      errorcode::proto::UNKNOWN_STOP_ADVERTISING_ERROR;
  StartDiscoveringError start_discovering_error =
      errorcode::proto::UNKNOWN_START_DISCOVERING_ERROR;
  StopDiscoveringError stop_discovering_error =
      errorcode::proto::UNKNOWN_STOP_DISCOVERING_ERROR;
  StartListeningIncomingConnectionError
      start_listening_incoming_connection_error =
          errorcode::proto::UNKNOWN_START_LISTENING_INCOMING_CONNECTION_ERROR;
  StopListeningIncomingConnectionError
      stop_listening_incoming_connection_error =
          errorcode::proto::UNKNOWN_STOP_LISTENING_INCOMING_CONNECTION_ERROR;
  std::string connection_token = {};
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_ERROR_CORE_PARAMS_H_
