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

#include "connections/listeners.h"

#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "connections/medium_selector.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "connections/v3/bandwidth_info.h"
#include "connections/v3/connection_result.h"
#include "connections/v3/connections_device.h"
#include "connections/v3/listeners.h"
#include "internal/interop/device.h"
#include "internal/platform/byte_array.h"

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
  listener.bandwidth_changed_cb(endpoint_id, Medium());
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
  listener.bandwidth_changed_cb(endpoint_id, Medium());
  EXPECT_TRUE(initiated_cb_called);
}

TEST(ListenersTest, PayloadListener_PayloadCb_Works) {
  bool payload_content_match = false;
  const std::string input_bytes = "input";
  Payload payload = Payload(ByteArray(input_bytes));

  PayloadListener listener{
      .payload_cb =
          [&](absl::string_view endpoint_id, Payload payload) {
            if (payload.AsBytes().data() == input_bytes) {
              payload_content_match = true;
            }
          },
  };
  listener.payload_cb("endpoint_id", std::move(payload));

  EXPECT_TRUE(payload_content_match);
}

TEST(ListenersTest, ConstDiscoveryAndPayloadListenersAreCallable) {
  int callbacks_invoked = 0;
  const DiscoveryListener discovery_listener{
      .endpoint_found_cb =
          [&](const std::string&, const ByteArray&, const std::string&) {
            ++callbacks_invoked;
          },
      .endpoint_lost_cb = [&](const std::string&) { ++callbacks_invoked; },
      .endpoint_distance_changed_cb =
          [&](const std::string&, DistanceInfo) { ++callbacks_invoked; },
  };
  discovery_listener.endpoint_found_cb("ep", ByteArray("info"), "svc");
  discovery_listener.endpoint_lost_cb("ep");
  discovery_listener.endpoint_distance_changed_cb("ep", DistanceInfo::kClose);
  EXPECT_EQ(callbacks_invoked, 3);

  const PayloadListener payload_listener{
      .payload_cb =
          [&](absl::string_view, Payload) { ++callbacks_invoked; },
      .payload_progress_cb =
          [&](absl::string_view, const PayloadProgressInfo&) {
            ++callbacks_invoked;
          },
  };
  payload_listener.payload_cb("ep", Payload(ByteArray("data")));
  payload_listener.payload_progress_cb("ep", PayloadProgressInfo{});
  EXPECT_EQ(callbacks_invoked, 5);
}

TEST(ListenersTest, ConstV3ListenersAreCallable) {
  int callbacks_invoked = 0;
  v3::ConnectionsDevice device("ep", "info", {});

  const v3::ConnectionListener connection_listener{
      .initiated_cb =
          [&](const NearbyDevice&, const v3::InitialConnectionInfo&) {
            ++callbacks_invoked;
          },
      .result_cb =
          [&](const NearbyDevice&, v3::ConnectionResult) {
            ++callbacks_invoked;
          },
      .disconnected_cb = [&](const NearbyDevice&) { ++callbacks_invoked; },
      .bandwidth_changed_cb =
          [&](const NearbyDevice&, v3::BandwidthInfo) { ++callbacks_invoked; },
  };
  connection_listener.initiated_cb(device, {});
  connection_listener.result_cb(device, {});
  connection_listener.disconnected_cb(device);
  connection_listener.bandwidth_changed_cb(device, {});
  EXPECT_EQ(callbacks_invoked, 4);

  const v3::DiscoveryListener discovery_listener{
      .endpoint_found_cb =
          [&](const NearbyDevice&, absl::string_view) { ++callbacks_invoked; },
      .endpoint_lost_cb = [&](const NearbyDevice&) { ++callbacks_invoked; },
      .endpoint_distance_changed_cb =
          [&](const NearbyDevice&, DistanceInfo) { ++callbacks_invoked; },
  };
  discovery_listener.endpoint_found_cb(device, "svc");
  discovery_listener.endpoint_lost_cb(device);
  discovery_listener.endpoint_distance_changed_cb(device, DistanceInfo::kClose);
  EXPECT_EQ(callbacks_invoked, 7);

  const v3::PayloadListener payload_listener{
      .payload_received_cb =
          [&](const NearbyDevice&, Payload) { ++callbacks_invoked; },
      .payload_progress_cb =
          [&](const NearbyDevice&, const PayloadProgressInfo&) {
            ++callbacks_invoked;
          },
  };
  payload_listener.payload_received_cb(device, Payload(ByteArray("data")));
  payload_listener.payload_progress_cb(device, PayloadProgressInfo{});
  EXPECT_EQ(callbacks_invoked, 9);
}

}  // namespace
}  // namespace connections
}  // namespace nearby
