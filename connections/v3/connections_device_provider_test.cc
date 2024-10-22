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

class MockAuthenticationTransport : public AuthenticationTransport {
  MOCK_METHOD(void, WriteMessage, (absl::string_view), (const, override));
  MOCK_METHOD(std::string, ReadMessage, (), (const, override));
};

TEST(ConnectionsDeviceProviderTest, TestProviderWorksTwoArgs) {
  ConnectionsDeviceProvider provider(kEndpointInfo, {});
  auto device = provider.GetLocalDevice();
  ASSERT_EQ(device->GetType(), NearbyDevice::Type::kConnectionsDevice);
  auto connections_device = static_cast<const ConnectionsDevice*>(device);
  EXPECT_EQ(connections_device->GetEndpointInfo(), kEndpointInfo);
  EXPECT_EQ(connections_device->GetConnectionInfos().size(), 0);
  EXPECT_EQ(connections_device->GetEndpointId().size(), kEndpointIdLength);
}

TEST(ConnectionsDeviceProviderTest, TestProviderWorksThreeArgs) {
  ConnectionsDeviceProvider provider(kEndpointId, kEndpointInfo, {});
  auto device = provider.GetLocalDevice();
  ASSERT_EQ(device->GetType(), NearbyDevice::Type::kConnectionsDevice);
  auto connections_device = static_cast<const ConnectionsDevice*>(device);
  EXPECT_EQ(connections_device->GetEndpointInfo(), kEndpointInfo);
  EXPECT_EQ(connections_device->GetConnectionInfos().size(), 0);
  EXPECT_EQ(connections_device->GetEndpointId(), kEndpointId);
}

TEST(ConnectionsDeviceProviderTest, TestUnknownAuthStatus) {
  ConnectionsDeviceProvider provider(kEndpointId, kEndpointInfo, {});
  MockAuthenticationTransport transport;
  EXPECT_EQ(provider.AuthenticateAsInitiator(ConnectionsDevice("", {}),
                                             /*shared_secret=*/"", transport),
            AuthenticationStatus::kUnknown);
  EXPECT_EQ(provider.AuthenticateAsResponder(/*shared_secret=*/"", transport),
            AuthenticationStatus::kUnknown);
}

}  // namespace
}  // namespace v3
}  // namespace connections
}  // namespace nearby
