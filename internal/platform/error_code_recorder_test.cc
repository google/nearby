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

#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {

using ::testing::Field;
using ::testing::MockFunction;
using ::testing::StrictMock;

TEST(ErrorCodeRecorderTest, TestListenerWork) {
  StrictMock<MockFunction<void(const ErrorCodeParams& params)>> mock_listener;
  EXPECT_CALL(mock_listener, Call).Times(1);
  ErrorCodeRecorder error_code_recorder(mock_listener.AsStdFunction());

  ErrorCodeRecorder::LogErrorCode(
      location::nearby::proto::connections::BLE,
      location::nearby::errorcode::proto::START_ADVERTISING,
      location::nearby::errorcode::proto::
          MULTIPLE_FAST_ADVERTISEMENT_NOT_ALLOWED,
      location::nearby::errorcode::proto::FEATURE_BLUETOOTH_NOT_SUPPORTED,
      "pii_message", "connection_token");
}

TEST(ErrorCodeRecorderTest, TestBuildErrorCodeParamsWork) {
  std::string pii_message = "pii_message";
  std::string connection_token = "connection_token";

  ErrorCodeParams error_code_params;
  ErrorCodeRecorder error_recorder(
      [&error_code_params](const ErrorCodeParams& params) {
        error_code_params = std::move(params);
      });

  ErrorCodeRecorder::LogErrorCode(
      location::nearby::proto::connections::BLE,
      location::nearby::errorcode::proto::START_ADVERTISING,
      location::nearby::errorcode::proto::
          MULTIPLE_FAST_ADVERTISEMENT_NOT_ALLOWED,
      location::nearby::errorcode::proto::FEATURE_BLUETOOTH_NOT_SUPPORTED,
      pii_message, connection_token);

  EXPECT_THAT(
      error_code_params,
      AllOf(Field("medium", &ErrorCodeParams::medium,
                  location::nearby::proto::connections::BLE),
            Field("event", &ErrorCodeParams::event,
                  location::nearby::errorcode::proto::START_ADVERTISING),
            Field("description", &ErrorCodeParams::description,
                  location::nearby::errorcode::proto::
                      FEATURE_BLUETOOTH_NOT_SUPPORTED),
            Field("pii_message", &ErrorCodeParams::pii_message, pii_message),
            Field("is_common_error", &ErrorCodeParams::is_common_error, false),
            Field("start_advertising_error",
                  &ErrorCodeParams::start_advertising_error,
                  location::nearby::errorcode::proto::
                      MULTIPLE_FAST_ADVERTISEMENT_NOT_ALLOWED),
            Field("connection_token", &ErrorCodeParams::connection_token,
                  connection_token)));
}

TEST(ErrorCodeRecorderTest, TestBuildErrorCodeParamsWorkForCommonError) {
  std::string pii_message = "pii_message";
  std::string connection_token = "connection_token";

  ErrorCodeParams error_code_params;
  ErrorCodeRecorder error_recorder(
      [&error_code_params](const ErrorCodeParams& params) {
        error_code_params = std::move(params);
      });

  ErrorCodeRecorder::LogErrorCode(
      location::nearby::proto::connections::BLE,
      location::nearby::errorcode::proto::START_ADVERTISING,
      location::nearby::errorcode::proto::INVALID_PARAMETER,
      location::nearby::errorcode::proto::NULL_SERVICE_ID, pii_message,
      connection_token);

  EXPECT_THAT(
      error_code_params,
      AllOf(Field("medium", &ErrorCodeParams::medium,
                  location::nearby::proto::connections::BLE),
            Field("event", &ErrorCodeParams::event,
                  location::nearby::errorcode::proto::START_ADVERTISING),
            Field("description", &ErrorCodeParams::description,
                  location::nearby::errorcode::proto::NULL_SERVICE_ID),
            Field("pii_message", &ErrorCodeParams::pii_message, pii_message),
            Field("is_common_error", &ErrorCodeParams::is_common_error, true),
            Field("common_error", &ErrorCodeParams::common_error,
                  location::nearby::errorcode::proto::INVALID_PARAMETER),
            Field("connection_token", &ErrorCodeParams::connection_token,
                  connection_token)));
}

TEST(ErrorCodeRecorderTest, TestBuildErrorCodeParamsWorkForUnknownEvent) {
  std::string pii_message = "pii_message";
  std::string connection_token = "connection_token";

  ErrorCodeParams error_code_params;
  ErrorCodeRecorder error_recorder(
      [&error_code_params](const ErrorCodeParams& params) {
        error_code_params = std::move(params);
      });

  // If event is UNKNOWN_EVENT and the error is not Common_Error then the error
  // will be set to UNKNOWN_ERROR.
  ErrorCodeRecorder::LogErrorCode(
      location::nearby::proto::connections::BLE,
      location::nearby::errorcode::proto::UNKNOWN_EVENT,
      location::nearby::errorcode::proto::
          MULTIPLE_FAST_ADVERTISEMENT_NOT_ALLOWED,
      location::nearby::errorcode::proto::FEATURE_BLUETOOTH_NOT_SUPPORTED,
      pii_message, connection_token);

  EXPECT_THAT(
      error_code_params,
      AllOf(Field("medium", &ErrorCodeParams::medium,
                  location::nearby::proto::connections::BLE),
            Field("event", &ErrorCodeParams::event,
                  location::nearby::errorcode::proto::UNKNOWN_EVENT),
            Field("description", &ErrorCodeParams::description,
                  location::nearby::errorcode::proto::
                      FEATURE_BLUETOOTH_NOT_SUPPORTED),
            Field("pii_message", &ErrorCodeParams::pii_message, pii_message),
            Field("is_common_error", &ErrorCodeParams::is_common_error, true),
            Field("common_error", &ErrorCodeParams::common_error,
                  location::nearby::errorcode::proto::UNKNOWN_ERROR),
            Field("connection_token", &ErrorCodeParams::connection_token,
                  connection_token)));
}

}  // namespace nearby
