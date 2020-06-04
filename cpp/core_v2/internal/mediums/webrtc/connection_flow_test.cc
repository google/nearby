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
