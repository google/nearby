#include "core/internal/endpoint_channel_manager.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {

TEST(EndpointChannelManagerTest, ConstructorDestructorWorks) {
  EndpointChannelManager mgr;
  SUCCEED();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
