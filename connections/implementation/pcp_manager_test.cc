// Copyright 2021 Google LLC
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

#include "connections/implementation/pcp_manager.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "connections/advertising_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/mock_device.h"
#include "connections/implementation/simulation_user.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/out_of_band_connection_metadata.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "connections/v3/connection_listening_options.h"
#include "connections/v3/connection_result.h"
#include "connections/v3/listeners.h"
#include "internal/interop/device.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace connections {
namespace {

using ::testing::Return;

constexpr std::array<char, 6> kFakeMacAddress = {'a', 'b', 'c', 'd', 'e', 'f'};
constexpr char kServiceId[] = "service-id";
constexpr char kDeviceA[] = "device-A";
constexpr char kDeviceB[] = "device-B";

constexpr BooleanMediumSelector kTestCases[] = {
    BooleanMediumSelector{
        .ble = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
    },
    BooleanMediumSelector{
        .wifi_lan = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
        .ble = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
        .wifi_lan = true,
    },
    BooleanMediumSelector{
        .ble = true,
        .wifi_lan = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
        .ble = true,
        .wifi_lan = true,
    },
};

class PcpManagerTest : public ::testing::TestWithParam<BooleanMediumSelector> {
 protected:
  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(PcpManagerTest, CanCreateOne) {
  env_.Start();
  SimulationUser user(kDeviceA, GetParam());
  env_.Stop();
}

TEST_P(PcpManagerTest, CanCreateMany) {
  env_.Start();
  SimulationUser user_a(kDeviceA, GetParam());
  SimulationUser user_b(kDeviceB, GetParam());
  env_.Stop();
}

TEST_P(PcpManagerTest, CanAdvertise) {
  env_.Start();
  SimulationUser user_a(kDeviceA, GetParam());
  SimulationUser user_b(kDeviceB, GetParam());
  user_a.StartAdvertising(kServiceId, nullptr);
  env_.Stop();
}

TEST_P(PcpManagerTest, CanDiscover) {
  env_.Start();
  SimulationUser user_a("device-a", GetParam());
  SimulationUser user_b("device-b", GetParam());
  user_a.StartAdvertising(kServiceId, nullptr);
  CountDownLatch latch(1);
  user_b.StartDiscovery(kServiceId, &latch);
  EXPECT_TRUE(latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_EQ(user_b.GetDiscovered().service_id, kServiceId);
  EXPECT_EQ(user_b.GetDiscovered().endpoint_info, user_a.GetInfo());
  user_b.StopDiscovery();
  env_.Stop();
}

TEST_P(PcpManagerTest, CanConnect) {
  env_.Start();
  SimulationUser user_a("device-a", GetParam());
  SimulationUser user_b("device-b", GetParam());
  CountDownLatch discovery_latch(1);
  CountDownLatch connection_latch(2);
  user_a.StartAdvertising(kServiceId, &connection_latch);
  user_b.StartDiscovery(kServiceId, &discovery_latch);
  EXPECT_TRUE(discovery_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_EQ(user_b.GetDiscovered().service_id, kServiceId);
  EXPECT_EQ(user_b.GetDiscovered().endpoint_info, user_a.GetInfo());
  user_b.RequestConnection(&connection_latch);
  EXPECT_TRUE(connection_latch.Await(absl::Milliseconds(1000)).result());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_P(PcpManagerTest, CanAccept) {
  env_.Start();
  SimulationUser user_a("device-a", GetParam());
  SimulationUser user_b("device-b", GetParam());
  CountDownLatch discovery_latch(1);
  CountDownLatch connection_latch(2);
  CountDownLatch accept_latch(2);
  user_a.StartAdvertising(kServiceId, &connection_latch);
  user_b.StartDiscovery(kServiceId, &discovery_latch);
  EXPECT_TRUE(discovery_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_EQ(user_b.GetDiscovered().service_id, kServiceId);
  EXPECT_EQ(user_b.GetDiscovered().endpoint_info, user_a.GetInfo());
  user_b.RequestConnection(&connection_latch);
  EXPECT_TRUE(connection_latch.Await(absl::Milliseconds(1000)).result());
  user_a.AcceptConnection(&accept_latch);
  user_b.AcceptConnection(&accept_latch);
  EXPECT_TRUE(accept_latch.Await(absl::Milliseconds(1000)).result());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_P(PcpManagerTest, CanReject) {
  env_.Start();
  SimulationUser user_a("device-a", GetParam());
  SimulationUser user_b("device-b", GetParam());
  CountDownLatch discovery_latch(1);
  CountDownLatch connection_latch(2);
  CountDownLatch reject_latch(1);
  user_a.StartAdvertising(kServiceId, &connection_latch);
  user_b.StartDiscovery(kServiceId, &discovery_latch);
  EXPECT_TRUE(discovery_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_EQ(user_b.GetDiscovered().service_id, kServiceId);
  EXPECT_EQ(user_b.GetDiscovered().endpoint_info, user_a.GetInfo());
  user_b.RequestConnection(&connection_latch);
  EXPECT_TRUE(connection_latch.Await(absl::Milliseconds(1000)).result());
  user_b.ExpectRejectedConnection(reject_latch);
  user_a.RejectConnection(nullptr);
  EXPECT_TRUE(reject_latch.Await(absl::Milliseconds(1000)).result());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_P(PcpManagerTest, CanStartListeningForIncomingConnections) {
  env_.Start();
  BooleanMediumSelector selector = GetParam();
  SimulationUser user_a(kDeviceA, selector);
  CountDownLatch start_latch(1);
  v3::ConnectionListeningOptions options;
  options.enable_ble_listening = selector.ble;
  options.enable_bluetooth_listening = selector.bluetooth;
  options.enable_wlan_listening = selector.wifi_lan;
  options.listening_mediums = selector.GetMediums(true);
  options.upgrade_mediums = selector.GetMediums(true);
  options.strategy = Strategy::kP2pCluster;
  user_a.StartListeningForIncomingConnections(&start_latch, "service", options,
                                              {Status::kSuccess});
  user_a.Stop();
  env_.Stop();
}

TEST_P(PcpManagerTest, StartListeningForIncomingConnectionsFailsNoStrategy) {
  env_.Start();
  BooleanMediumSelector selector = GetParam();
  SimulationUser user_a(kDeviceA, selector);
  CountDownLatch start_latch(1);
  v3::ConnectionListeningOptions options;
  options.enable_ble_listening = selector.ble;
  options.enable_bluetooth_listening = selector.bluetooth;
  options.enable_wlan_listening = selector.wifi_lan;
  options.listening_mediums = selector.GetMediums(true);
  options.upgrade_mediums = selector.GetMediums(true);
  user_a.StartListeningForIncomingConnections(&start_latch, "service", options,
                                              {Status::kError});
  user_a.Stop();
  env_.Stop();
}

TEST_P(PcpManagerTest, CanConnectV3) {
  env_.Start();
  SimulationUser user_a("device-a", GetParam());
  SimulationUser user_b("device-b", GetParam());
  CountDownLatch discovery_latch(1);
  CountDownLatch connection_latch(2);
  user_a.StartAdvertising(kServiceId, &connection_latch);
  user_b.StartDiscovery(kServiceId, &discovery_latch);
  EXPECT_TRUE(discovery_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_EQ(user_b.GetDiscovered().service_id, kServiceId);
  EXPECT_EQ(user_b.GetDiscovered().endpoint_info, user_a.GetInfo());
  auto remote_device = MockNearbyDevice();
  EXPECT_CALL(remote_device, GetEndpointId)
      .WillRepeatedly(Return(std::string(user_b.GetDiscovered().endpoint_id)));
  user_b.RequestConnectionV3(&connection_latch, remote_device);
  EXPECT_TRUE(connection_latch.Await(absl::Milliseconds(1000)).result());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedPcpManagerTest, PcpManagerTest,
                         ::testing::ValuesIn(kTestCases));

// Verifies that InjectEndpoint() can be run successfully; does not test the
// full connection flow given that normal discovery/advertisement is skipped.
// Note: Not parameterized because InjectEndpoint only works over Bluetooth.
TEST_F(PcpManagerTest, InjectEndpoint) {
  env_.Start();
  SimulationUser user_a(kDeviceA, BooleanMediumSelector{.bluetooth = true});
  user_a.StartDiscovery(kServiceId, /*latch=*/nullptr);
  user_a.InjectEndpoint(kServiceId, OutOfBandConnectionMetadata{
                                        .medium = Medium::BLUETOOTH,
                                        .remote_bluetooth_mac_address =
                                            ByteArray(kFakeMacAddress),
                                    });
  user_a.StopDiscovery();
  user_a.Stop();
  env_.Stop();
}

TEST_F(PcpManagerTest, TestUpdateDiscoveryFailsWithoutPcpHandler) {
  BooleanMediumSelector selector;
  selector.SetAll(true);
  env_.Start();
  SimulationUser user_a(kDeviceA, selector);
  EXPECT_EQ(user_a.UpdateDiscoveryOptions(kServiceId).value,
            Status::Value::kOutOfOrderApiCall);
  env_.Stop();
}

TEST_F(PcpManagerTest, TestUpdateAdvertisingFailsWithoutPcpHandler) {
  BooleanMediumSelector selector;
  selector.SetAll(true);
  env_.Start();
  SimulationUser user_a(kDeviceA, selector);
  EXPECT_EQ(user_a.GetPcpManager()
                .UpdateAdvertisingOptions(&user_a.GetClientProxy(), kServiceId,
                                          AdvertisingOptions{})
                .value,
            Status::Value::kOutOfOrderApiCall);
  env_.Stop();
}

TEST_F(PcpManagerTest, TestAcceptConnectionReturnsEndpointUnknown) {
  BooleanMediumSelector selector;
  selector.SetAll(true);
  env_.Start();
  SimulationUser user_a(kDeviceA, selector);
  EXPECT_EQ(
      user_a.GetPcpManager()
          .AcceptConnection(&user_a.GetClientProxy(), "unknown_endpoint", {})
          .value,
      Status::Value::kEndpointUnknown);
  env_.Stop();
}

TEST_F(PcpManagerTest, TestRejectConnectionReturnsEndpointUnknown) {
  BooleanMediumSelector selector;
  selector.SetAll(true);
  env_.Start();
  SimulationUser user_a(kDeviceA, selector);
  EXPECT_EQ(user_a.GetPcpManager()
                .RejectConnection(&user_a.GetClientProxy(), "unknown_endpoint")
                .value,
            Status::Value::kEndpointUnknown);
  env_.Stop();
}

TEST_F(PcpManagerTest, TestRequestConnectionReturnsEndpointUnknown) {
  BooleanMediumSelector selector;
  selector.SetAll(true);
  env_.Start();
  SimulationUser user_a(kDeviceA, selector);
  EXPECT_EQ(user_a.GetPcpManager()
                .RequestConnection(&user_a.GetClientProxy(), "unknown_endpoint",
                                   {}, {})
                .value,
            Status::Value::kEndpointUnknown);
  env_.Stop();
}

TEST_F(PcpManagerTest, TestRequestConnectionV3ReturnsEndpointUnknown) {
  BooleanMediumSelector selector;
  selector.SetAll(true);
  env_.Start();
  SimulationUser user_a(kDeviceA, selector);
  auto remote_device = MockNearbyDevice();
  EXPECT_CALL(remote_device, GetEndpointId)
      .WillRepeatedly(Return("unknown_endpoint"));
  EXPECT_EQ(
      user_a.GetPcpManager()
          .RequestConnectionV3(&user_a.GetClientProxy(), remote_device, {}, {})
          .value,
      Status::Value::kEndpointUnknown);
  env_.Stop();
}

TEST_F(PcpManagerTest, MixedTopologiesCorrectlyRouted) {
  BooleanMediumSelector selector;
  selector.wifi_lan = true;
  env_.Start();
  SimulationUser user_a(kDeviceA, selector);

  // Start advertising with kP2pStar
  AdvertisingOptions advertising_options;
  advertising_options.strategy = Strategy::kP2pStar;
  advertising_options.allowed = selector;

  ConnectionListener life_cycle_listener = {
      .initiated_cb = [](const std::string&, const ConnectionResponseInfo&) {},
      .accepted_cb = [](const std::string&) {},
      .rejected_cb = [](const std::string&, const Status&) {},
  };

  EXPECT_TRUE(user_a.GetPcpManager()
                  .StartAdvertising(
                      &user_a.GetClientProxy(), kServiceId, advertising_options,
                      {
                          .endpoint_info = user_a.GetInfo(),
                          .listener = std::move(life_cycle_listener),
                      })
                  .Ok());

  EXPECT_TRUE(user_a.GetClientProxy().IsAdvertising());

  // Start discovery with kP2pPointToPoint
  DiscoveryOptions discovery_options;
  discovery_options.strategy = Strategy::kP2pPointToPoint;
  discovery_options.allowed = selector;

  DiscoveryListener discovery_listener = {
      .endpoint_found_cb = [](const std::string&, const ByteArray&,
                              const std::string&) {},
      .endpoint_lost_cb = [](const std::string&) {},
  };

  EXPECT_TRUE(user_a.GetPcpManager()
                  .StartDiscovery(&user_a.GetClientProxy(), kServiceId,
                                  discovery_options,
                                  std::move(discovery_listener))
                  .Ok());

  EXPECT_TRUE(user_a.GetClientProxy().IsDiscovering());

  // Stop advertising. Under the old code, this was misrouted and left
  // IsAdvertising() as true.
  user_a.GetPcpManager().StopAdvertising(&user_a.GetClientProxy());
  EXPECT_FALSE(user_a.GetClientProxy().IsAdvertising());

  // Stop discovery
  user_a.GetPcpManager().StopDiscovery(&user_a.GetClientProxy());
  EXPECT_FALSE(user_a.GetClientProxy().IsDiscovering());

  env_.Stop();
}

TEST_F(PcpManagerTest, ListenAndDiscoverMixedTopologiesCorrectlyRouted) {
  BooleanMediumSelector selector;
  selector.wifi_lan = true;
  env_.Start();
  SimulationUser user_a(kDeviceA, selector);

  // Start listening with kP2pStar
  v3::ConnectionListeningOptions listening_options;
  listening_options.strategy = Strategy::kP2pStar;

  v3::ConnectionListener listener = {
      .initiated_cb = [](const NearbyDevice&,
                         const v3::InitialConnectionInfo&) {},
      .result_cb = [](const NearbyDevice&, v3::ConnectionResult) {},
      .disconnected_cb = [](const NearbyDevice&) {},
  };

  auto listening_result =
      user_a.GetPcpManager().StartListeningForIncomingConnections(
          &user_a.GetClientProxy(), kServiceId, std::move(listener),
          listening_options);
  EXPECT_TRUE(listening_result.first.Ok());
  EXPECT_TRUE(user_a.GetClientProxy().IsListeningForIncomingConnections());

  // Start discovery with kP2pPointToPoint
  DiscoveryOptions discovery_options;
  discovery_options.strategy = Strategy::kP2pPointToPoint;
  discovery_options.allowed = selector;

  DiscoveryListener discovery_listener = {
      .endpoint_found_cb = [](const std::string&, const ByteArray&,
                              const std::string&) {},
      .endpoint_lost_cb = [](const std::string&) {},
  };

  EXPECT_TRUE(user_a.GetPcpManager()
                  .StartDiscovery(&user_a.GetClientProxy(), kServiceId,
                                  discovery_options,
                                  std::move(discovery_listener))
                  .Ok());

  EXPECT_TRUE(user_a.GetClientProxy().IsDiscovering());

  // Stop listening. Under the old code, this was misrouted and left
  // IsListeningForIncomingConnections() as true.
  user_a.GetPcpManager().StopListeningForIncomingConnections(
      &user_a.GetClientProxy());
  EXPECT_FALSE(user_a.GetClientProxy().IsListeningForIncomingConnections());

  // Stop discovery
  user_a.GetPcpManager().StopDiscovery(&user_a.GetClientProxy());
  EXPECT_FALSE(user_a.GetClientProxy().IsDiscovering());

  env_.Stop();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
