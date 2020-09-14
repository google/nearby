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

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
