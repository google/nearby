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

#include "connections/cpp/implementation/p2p_cluster_pcp_handler.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "connections/cpp/implementation/bwu_manager.h"
#include "connections/cpp/implementation/injected_bluetooth_device_store.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace connections {
namespace {

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

// Combines the bool `support_ble_v2` as param testing but should revert it back
// if ble_v2 is done and ble will be replaced by ble_v2.
class P2pClusterPcpHandlerTest
    : public testing::TestWithParam<std::tuple<BooleanMediumSelector, bool>> {
 protected:
  void SetUp() override {
    NEARBY_LOG(INFO, "SetUp: begin");
    FeatureFlags::GetMutableFlagsForTesting().support_ble_v2 =
        std::get<1>(GetParam());
    if (advertising_options_.allowed.ble) {
      NEARBY_LOG(INFO, "SetUp: BLE enabled");
    }
    if (advertising_options_.allowed.bluetooth) {
      NEARBY_LOG(INFO, "SetUp: BT enabled");
    }
    if (advertising_options_.allowed.wifi_lan) {
      NEARBY_LOG(INFO, "SetUp: WifiLan enabled");
    }
    if (advertising_options_.allowed.web_rtc) {
      NEARBY_LOG(INFO, "SetUp: WebRTC enabled");
    }
    NEARBY_LOG(INFO, "SetUp: end");
  }

  ClientProxy client_a_;
  ClientProxy client_b_;
  std::string service_id_{"service"};
  ConnectionOptions connection_options_{
      {
          Strategy::kP2pCluster,
          std::get<0>(GetParam()),
      },
  };
  AdvertisingOptions advertising_options_{
      {
          Strategy::kP2pCluster,
          std::get<0>(GetParam()),
      },
  };
  DiscoveryOptions discovery_options_{
      {
          Strategy::kP2pCluster,
          std::get<0>(GetParam()),
      },
  };
  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(P2pClusterPcpHandlerTest, CanConstructOne) {
  env_.Start();
  Mediums mediums;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(mediums, em, ecm, {}, {});
  InjectedBluetoothDeviceStore ibds;
  P2pClusterPcpHandler handler(&mediums, &em, &ecm, &bwu, ibds);
  env_.Stop();
}

TEST_P(P2pClusterPcpHandlerTest, CanConstructMultiple) {
  env_.Start();
  Mediums mediums_a;
  Mediums mediums_b;
  EndpointChannelManager ecm_a;
  EndpointChannelManager ecm_b;
  EndpointManager em_a(&ecm_a);
  EndpointManager em_b(&ecm_b);
  BwuManager bwu_a(mediums_a, em_a, ecm_a, {}, {});
  BwuManager bwu_b(mediums_b, em_b, ecm_b, {}, {});
  InjectedBluetoothDeviceStore ibds_a;
  InjectedBluetoothDeviceStore ibds_b;
  P2pClusterPcpHandler handler_a(&mediums_a, &em_a, &ecm_a, &bwu_a, ibds_a);
  P2pClusterPcpHandler handler_b(&mediums_b, &em_b, &ecm_b, &bwu_b, ibds_b);
  env_.Stop();
}

TEST_P(P2pClusterPcpHandlerTest, CanAdvertise) {
  env_.Start();
  std::string endpoint_name{"endpoint_name"};
  Mediums mediums_a;
  EndpointChannelManager ecm_a;
  EndpointManager em_a(&ecm_a);
  BwuManager bwu_a(mediums_a, em_a, ecm_a, {}, {});
  InjectedBluetoothDeviceStore ibds_a;
  P2pClusterPcpHandler handler_a(&mediums_a, &em_a, &ecm_a, &bwu_a, ibds_a);
  EXPECT_EQ(
      handler_a.StartAdvertising(&client_a_, service_id_, advertising_options_,
                                 {.endpoint_info = ByteArray{endpoint_name}}),
      Status{Status::kSuccess});
  env_.Stop();
}

TEST_P(P2pClusterPcpHandlerTest, CanDiscover) {
  env_.Start();
  std::string endpoint_name{"endpoint_name"};
  Mediums mediums_a;
  Mediums mediums_b;
  EndpointChannelManager ecm_a;
  EndpointChannelManager ecm_b;
  EndpointManager em_a(&ecm_a);
  EndpointManager em_b(&ecm_b);
  BwuManager bwu_a(mediums_a, em_a, ecm_a, {}, {});
  BwuManager bwu_b(mediums_b, em_b, ecm_b, {}, {});
  InjectedBluetoothDeviceStore ibds_a;
  InjectedBluetoothDeviceStore ibds_b;
  P2pClusterPcpHandler handler_a(&mediums_a, &em_a, &ecm_a, &bwu_a, ibds_a);
  P2pClusterPcpHandler handler_b(&mediums_b, &em_b, &ecm_b, &bwu_b, ibds_b);
  CountDownLatch latch(1);
  EXPECT_EQ(
      handler_a.StartAdvertising(&client_a_, service_id_, advertising_options_,
                                 {.endpoint_info = ByteArray{endpoint_name}}),
      Status{Status::kSuccess});
  EXPECT_EQ(handler_b.StartDiscovery(
                &client_b_, service_id_, discovery_options_,
                {
                    .endpoint_found_cb =
                        [&latch](const std::string& endpoint_id,
                                 const ByteArray& endpoint_info,
                                 const std::string& service_id) {
                          NEARBY_LOG(INFO, "Device discovered: id=%s",
                                     endpoint_id.c_str());
                          latch.CountDown();
                        },
                }),
            Status{Status::kSuccess});
  EXPECT_TRUE(latch.Await(absl::Milliseconds(1000)).result());
  // We discovered endpoint over one medium. Before we finish the test, we have
  // to stop discovery for other mediums that may be still ongoing.
  handler_b.StopDiscovery(&client_b_);
  env_.Stop();
}

TEST_P(P2pClusterPcpHandlerTest, CanConnect) {
  env_.Start();
  std::string endpoint_name_a{"endpoint_name"};
  Mediums mediums_a;
  Mediums mediums_b;
  BluetoothRadio& radio_a = mediums_a.GetBluetoothRadio();
  BluetoothRadio& radio_b = mediums_b.GetBluetoothRadio();
  radio_a.GetBluetoothAdapter().SetName("BT Device A");
  radio_b.GetBluetoothAdapter().SetName("BT Device B");
  EndpointChannelManager ecm_a;
  EndpointChannelManager ecm_b;
  EndpointManager em_a(&ecm_a);
  EndpointManager em_b(&ecm_b);
  BwuManager bwu_a(mediums_a, em_a, ecm_a, {},
                   {.allow_upgrade_to = {.bluetooth = true}});
  BwuManager bwu_b(mediums_b, em_b, ecm_b, {},
                   {.allow_upgrade_to = {.bluetooth = true}});
  InjectedBluetoothDeviceStore ibds_a;
  InjectedBluetoothDeviceStore ibds_b;
  P2pClusterPcpHandler handler_a(&mediums_a, &em_a, &ecm_a, &bwu_a, ibds_a);
  P2pClusterPcpHandler handler_b(&mediums_b, &em_b, &ecm_b, &bwu_b, ibds_b);
  CountDownLatch discover_latch(1);
  CountDownLatch connect_latch(2);
  struct DiscoveredInfo {
    std::string endpoint_id;
    ByteArray endpoint_info;
    std::string service_id;
  } discovered;
  EXPECT_EQ(
      handler_a.StartAdvertising(
          &client_a_, service_id_, advertising_options_,
          {
              .endpoint_info = ByteArray{endpoint_name_a},
              .listener =
                  {
                      .initiated_cb =
                          [&connect_latch](const std::string& endpoint_id,
                                           const ConnectionResponseInfo& info) {
                            NEARBY_LOG(INFO,
                                       "StartAdvertising: initiated_cb called");
                            connect_latch.CountDown();
                          },
                  },
          }),
      Status{Status::kSuccess});
  EXPECT_EQ(handler_b.StartDiscovery(
                &client_b_, service_id_, discovery_options_,
                {
                    .endpoint_found_cb =
                        [&discover_latch, &discovered](
                            const std::string& endpoint_id,
                            const ByteArray& endpoint_info,
                            const std::string& service_id) {
                          NEARBY_LOG(
                              INFO,
                              "Device discovered: id=%s, endpoint_info=%s",
                              endpoint_id.c_str(),
                              std::string{endpoint_info}.c_str());
                          discovered = {
                              .endpoint_id = endpoint_id,
                              .endpoint_info = endpoint_info,
                              .service_id = service_id,
                          };
                          discover_latch.CountDown();
                        },
                }),
            Status{Status::kSuccess});

  EXPECT_TRUE(discover_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_EQ(endpoint_name_a, std::string{discovered.endpoint_info});

  const std::string kBssid = "34:36:3B:C7:8C:71";
  const std::int32_t kFreq = 5200;
  constexpr char kIp4Bytes[] = {(char)192, (char)168, (char)1, (char)37};
  const std::string kIpAddr4Bytes(kIp4Bytes);

  connection_options_.connection_info.supports_5_ghz = true;
  connection_options_.connection_info.bssid = kBssid;
  connection_options_.connection_info.ap_frequency = kFreq;
  connection_options_.connection_info.ip_address = kIpAddr4Bytes;

  client_b_.AddCancellationFlag(discovered.endpoint_id);
  handler_b.RequestConnection(
      &client_b_, discovered.endpoint_id,
      {.endpoint_info = discovered.endpoint_info,
       .listener =
           {
               .initiated_cb =
                   [&connect_latch](const std::string& endpoint_id,
                                    const ConnectionResponseInfo& info) {
                     NEARBY_LOG(INFO, "RequestConnection: initiated_cb called");
                     connect_latch.CountDown();
                   },
           }},
      connection_options_);
  std::string  client_b_local_endpoint = client_b_.GetLocalEndpointId();

  EXPECT_TRUE(connect_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_TRUE(client_b_.Is5GHzSupported(discovered.endpoint_id));
  EXPECT_EQ(client_b_.GetBssid(discovered.endpoint_id), kBssid);
  EXPECT_EQ(client_b_.GetApFrequency(discovered.endpoint_id), kFreq);
  EXPECT_EQ(client_b_.GetIPAddress(discovered.endpoint_id), kIpAddr4Bytes);
  EXPECT_EQ(client_a_.Is5GHzSupported(client_b_local_endpoint),
            mediums_b.GetWifi().GetCapability().supports_5_ghz);
  EXPECT_EQ(client_a_.GetBssid(client_b_local_endpoint),
            mediums_b.GetWifi().GetInformation().bssid);
  EXPECT_EQ(client_a_.GetApFrequency(client_b_local_endpoint),
            mediums_b.GetWifi().GetInformation().ap_frequency);
  EXPECT_EQ(client_a_.GetIPAddress(client_b_local_endpoint),
            mediums_b.GetWifi().GetInformation().ip_address_4_bytes);

  bwu_a.Shutdown();
  bwu_b.Shutdown();
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedPcpHandlerTest, P2pClusterPcpHandlerTest,
                         ::testing::Combine(::testing::ValuesIn(kTestCases),
                                            ::testing::Bool()));

}  // namespace
}  // namespace connections
}  // namespace nearby
