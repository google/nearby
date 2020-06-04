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

#include "core_v2/core.h"

#include "core_v2/internal/client_proxy.h"
#include "core_v2/internal/mock_service_controller.h"
#include "core_v2/internal/service_controller.h"
#include "platform_v2/public/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

TEST(CoreTest, ConstructorDestructorWorks) {
  MockServiceController mock;
  Core core{[&mock]() { return &mock; }};
}

TEST(CoreTest, DestructorReportsFatalFailure) {
  MockServiceController mock;
  ON_CALL(mock, StopDiscovery).WillByDefault([](ClientProxy* client) {
    NEARBY_LOG(INFO, "Blocking Endpoint disconnect for 10 sec");
    absl::SleepFor(absl::Milliseconds(10000));
  });
  ASSERT_DEATH(
      [&mock]() {
        Core core{[&mock]() { return &mock; }};
        EXPECT_CALL(mock, StartDiscovery).Times(1);
        EXPECT_CALL(mock, StopAdvertising).Times(1);
        core.StartDiscovery("service_id", {.strategy = Strategy::kP2pCluster},
                            {}, {.result_cb = [](Status status) {
                              NEARBY_LOG(INFO, "Discovery status: %d",
                                         static_cast<int>(status.value));
                            }});
      }(),
      "Unable to shutdown");
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
