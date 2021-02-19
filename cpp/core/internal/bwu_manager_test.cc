#include "core/internal/bwu_manager.h"

#include <string>

#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/mediums/mediums.h"
#include "core/internal/mediums/utils.h"
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

TEST(BwuManagerTest, CanProcessPathAvailableFrame) {
  ClientProxy client;
  std::string endpoint_id("EP_A");
  Mediums mediums;
  EndpointChannelManager ecm;
  EndpointManager em{&ecm};
  BwuManager bwu_manager{mediums, em, ecm, {}, {}};

  LocationHint location_hint = Utils::BuildLocationHint("US");
  ExceptionOr<OfflineFrame> wrapped_frame = parser::FromBytes(
      parser::ForBwuWebrtcPathAvailable("my_id", location_hint));

  bwu_manager.OnIncomingFrame(wrapped_frame.result(), endpoint_id, &client,
                              Medium::WEB_RTC);
  bwu_manager.Shutdown();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
