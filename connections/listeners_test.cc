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

#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
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

}  // namespace
}  // namespace connections
}  // namespace nearby
