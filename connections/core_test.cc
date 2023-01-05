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

#include "connections/core.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "connections/implementation/mock_service_controller_router.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {
namespace {

TEST(CoreTest, ConstructorDestructorWorks) {
  MockServiceControllerRouter mock;
  // Called when Core is destroyed.
  EXPECT_CALL(mock, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, const ResultCallback& callback) {
        callback.result_cb({Status::kSuccess});
      });
  Core core{&mock};
}

TEST(CoreTest, DestructorReportsFatalFailure) {
  ASSERT_DEATH(
      {
        MockServiceControllerRouter mock;
        // Never invoke the result callback so ~Core will time out.
        EXPECT_CALL(mock, StopAllEndpoints);
        Core core{&mock};
      },
      "Unable to shutdown");
}

}  // namespace
}  // namespace connections
}  // namespace nearby
