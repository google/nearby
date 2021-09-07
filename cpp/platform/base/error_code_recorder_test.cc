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

#include "platform/base/error_code_recorder.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {

using ::testing::MockFunction;
using ::testing::StrictMock;

TEST(ErrorCodeRecorderTest, TestListenerWork) {
  StrictMock<MockFunction<void(const ErrorCodeParams& params)>> mock_listener;
  ErrorCodeRecorder::ErrorCodeListener listener = mock_listener.AsStdFunction();
  EXPECT_CALL(mock_listener, Call).Times(1);
  ErrorCodeRecorder error_code_recorder(listener);

  ErrorCodeRecorder::LogErrorCode(
      proto::connections::BLE, errorcode::proto::START_ADVERTISING,
      errorcode::proto::MULTIPLE_FAST_ADVERTISEMENT_NOT_ALLOWED,
      errorcode::proto::FEATURE_BLUETOOTH_NOT_SUPPORTED, "pii_message",
      "connection_token");
}

}  // namespace nearby
}  // namespace location
