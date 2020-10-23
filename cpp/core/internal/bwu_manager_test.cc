#include "core/internal/bwu_manager.h"

#include <string>

#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/mediums/mediums.h"
#include "platform/public/system_clock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

TEST(BwuManagerTest, CanCreateInstance) {
  Mediums mediums;
  EndpointChannelManager ecm;
  EndpointManager em{&ecm};
  BwuManager bwu_manager{mediums, em, ecm, {}, {}};

  SystemClock::Sleep(absl::Seconds(3));

  bwu_manager.Shutdown();
}

TEST(BwuManagerTest, CanInitiateBwu) {
  ClientProxy client;
  std::string endpoint_id("EP_A");
  Mediums mediums;
  EndpointChannelManager ecm;
  EndpointManager em{&ecm};
  BwuManager bwu_manager{mediums, em, ecm, {}, {}};

  // Method returns void, so we just verify we did not SEGFAULT while calling.
  bwu_manager.InitiateBwuForEndpoint(&client, endpoint_id);
  SystemClock::Sleep(absl::Seconds(3));

  bwu_manager.Shutdown();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
