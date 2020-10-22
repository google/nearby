#include "core/internal/bwu_manager.h"

#include <string>

#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/mediums/mediums.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

TEST(BwuManagerTest, CanCreateInstance) {
  Mediums mediums;
  EndpointChannelManager ecm;
  EndpointManager em{&ecm};
  BwuManager bwu_manager{mediums, em, ecm, {}, {}};
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

  bwu_manager.Shutdown();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
