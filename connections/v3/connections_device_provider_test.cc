// Copyright 2023 Google LLC
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

#include "connections/v3/connections_device_provider.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace connections {
namespace v3 {
namespace {

constexpr absl::string_view kEndpointId = "ABCD";
constexpr absl::string_view kEndpointInfo = "NC endpoint";

TEST(ConnectionsDeviceProviderTest, TestProviderWorksTwoArgs) {
  ConnectionsDeviceProvider provider(kEndpointInfo, {});
  EXPECT_EQ(provider.GetLocalDevice().GetEndpointInfo(), kEndpointInfo);
  EXPECT_EQ(provider.GetLocalDevice().GetType(),
            NearbyDevice::Type::kConnectionsDevice);
  EXPECT_EQ(provider.GetLocalDevice().GetConnectionInfos().size(), 0);
  EXPECT_EQ(provider.GetLocalDevice().GetEndpointId().size(),
            kEndpointIdLength);
}

TEST(ConnectionsDeviceProviderTest, TestProviderWorksThreeArgs) {
  ConnectionsDeviceProvider provider(kEndpointId, kEndpointInfo, {});
  EXPECT_EQ(provider.GetLocalDevice().GetEndpointInfo(), kEndpointInfo);
  EXPECT_EQ(provider.GetLocalDevice().GetType(),
            NearbyDevice::Type::kConnectionsDevice);
  EXPECT_EQ(provider.GetLocalDevice().GetConnectionInfos().size(), 0);
  EXPECT_EQ(provider.GetLocalDevice().GetEndpointId(), kEndpointId);
}

}  // namespace
}  // namespace v3
}  // namespace connections
}  // namespace nearby
