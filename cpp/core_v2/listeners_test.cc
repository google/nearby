#include "core_v2/listeners.h"

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

TEST(ListenersTest, EnsureDefaultInitializedIsCallable) {
  ConnectionListener listener;
  std::string endpoint_id("endpoint_id");
  listener.initiated_cb(endpoint_id, ConnectionResponseInfo());
  listener.accepted_cb(endpoint_id);
  listener.rejected_cb(endpoint_id, {Status::kError});
  listener.disconnected_cb(endpoint_id);
  listener.bandwidth_changed_cb(endpoint_id, int());
  SUCCEED();
}

TEST(ListenersTest, EnsurePartiallyInitializedIsCallable) {
  std::string endpoint_id = {"endpoint_id"};
  bool initiated_cb_called = false;
  ConnectionListener listener{
      .initiated_cb =
          [&](std::string, ConnectionResponseInfo) {
            initiated_cb_called = true;
          },
  };
  listener.initiated_cb(endpoint_id, ConnectionResponseInfo());
  listener.accepted_cb(endpoint_id);
  listener.rejected_cb(endpoint_id, {Status::kError});
  listener.disconnected_cb(endpoint_id);
  listener.bandwidth_changed_cb(endpoint_id, int());
  EXPECT_TRUE(initiated_cb_called);
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
