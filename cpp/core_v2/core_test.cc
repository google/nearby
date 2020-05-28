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
