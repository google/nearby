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

#include "connections/v3/connections_device.h"

#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "internal/platform/ble_connection_info.h"
#include "internal/platform/connection_info.h"
#include "internal/platform/wifi_lan_connection_info.h"

namespace nearby {
namespace connections {
namespace v3 {
namespace {

constexpr absl::string_view kMacAddress = "\x34\x56\x67\x78\x89\x90";

std::vector<ConnectionInfoVariant> CreateDefaultConnectionInfos() {
  return {BleConnectionInfo(kMacAddress, "", "", {}),
          WifiLanConnectionInfo("\x94\xF4\x56\x13", "\x34\x56", {})};
}

TEST(ConnectionsDeviceTest, TestTwoArgumentConstructor) {
  auto connection_infos = CreateDefaultConnectionInfos();
  ConnectionsDevice device("connections endpoint", connection_infos);
  EXPECT_EQ(device.GetConnectionInfos(), connection_infos);
  EXPECT_EQ(device.GetType(), NearbyDevice::Type::kConnectionsDevice);
  EXPECT_EQ(device.GetEndpointId().length(), 4);
  EXPECT_EQ(device.GetEndpointInfo(), "connections endpoint");
}

TEST(ConnectionsDeviceTest, TestThreeArgumentConstructor) {
  auto connection_infos = CreateDefaultConnectionInfos();
  ConnectionsDevice device("ABCD", "connections endpoint", connection_infos);
  EXPECT_EQ(device.GetConnectionInfos(), connection_infos);
  EXPECT_EQ(device.GetType(), NearbyDevice::Type::kConnectionsDevice);
  EXPECT_EQ(device.GetEndpointId().length(), 4);
  EXPECT_EQ(device.GetEndpointId(), "ABCD");
  EXPECT_EQ(device.GetEndpointInfo(), "connections endpoint");
}

TEST(ConnectionsDeviceTest, TestLongEndpointId) {
  auto connection_infos = CreateDefaultConnectionInfos();
  ConnectionsDevice device("ABCDEF", "connections endpoint", connection_infos);
  EXPECT_EQ(device.GetConnectionInfos(), connection_infos);
  EXPECT_EQ(device.GetType(), NearbyDevice::Type::kConnectionsDevice);
  EXPECT_EQ(device.GetEndpointId().length(), 4);
  EXPECT_NE(device.GetEndpointId(), "ABCDEF");
  EXPECT_EQ(device.GetEndpointInfo(), "connections endpoint");
}

TEST(ConnectionsDeviceTest, TestToProtoBytes) {
  auto connection_infos = CreateDefaultConnectionInfos();
  ConnectionsDevice device("ABCD", "connections endpoint", connection_infos);
  auto proto_bytes = device.ToProtoBytes();
  location::nearby::connections::ConnectionsDevice device_frame;
  ASSERT_TRUE(device_frame.ParseFromString(proto_bytes));
  EXPECT_EQ(device_frame.endpoint_id(), "ABCD");
  EXPECT_EQ(device_frame.endpoint_info(), "connections endpoint");
  EXPECT_TRUE(device_frame.has_connectivity_info_list());
  EXPECT_EQ(device_frame.endpoint_type(),
            location::nearby::connections::CONNECTIONS_ENDPOINT);
}

}  // namespace
}  // namespace v3
}  // namespace connections
}  // namespace nearby
