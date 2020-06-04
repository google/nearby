#include "core_v2/internal/mediums/webrtc/connection_flow.h"

#include <memory>

#include "platform_v2/public/webrtc.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {
namespace {

TEST(ConnectionFlowTest, Create) {
  LocalIceCandidateListener local_ice_candidate_listener;
  DataChannelListener data_channel_listener;
  SingleThreadExecutor executor;
  WebRtcMedium webrtc_medium;

  std::unique_ptr<ConnectionFlow> connection_flow = ConnectionFlow::Create(
      std::move(local_ice_candidate_listener), std::move(data_channel_listener),
      &executor, webrtc_medium);

  EXPECT_NE(connection_flow, nullptr);
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
