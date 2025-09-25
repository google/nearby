// Copyright 2022 Google LLC
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

// #include
// "third_party/nearby/connections/implementation/p2p_cluster_pcp_handler.h"
#include "connections/implementation/p2p_point_to_point_pcp_handler.h"

#include <cstdint>
#include <string>
#include <tuple>

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/bwu_manager.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/injected_bluetooth_device_store.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/byte_array.h"
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
        .awdl = true,
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
    BooleanMediumSelector{
        .bluetooth = true,
        .ble = true,
        .wifi_lan = true,
        .wifi_hotspot = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
        .ble = true,
        .web_rtc = true,
        .wifi_lan = true,
        .wifi_direct = true,
        .awdl = true,
    },
};

class P2pPointToPointPcpHandlerTest
    : public testing::TestWithParam<std::tuple<BooleanMediumSelector>> {
 protected:
  void SetUp() override {
    LOG(INFO) << "SetUp: begin";
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::kEnableAwdl, true);
    if (advertising_options_.allowed.ble) {
      LOG(INFO) << "SetUp: BLE enabled";
    }
    if (advertising_options_.allowed.bluetooth) {
      LOG(INFO) << "SetUp: BT enabled";
    }
    if (advertising_options_.allowed.wifi_lan) {
      LOG(INFO) << "SetUp: WifiLan enabled";
    }
    if (advertising_options_.allowed.wifi_hotspot) {
      LOG(INFO) << "SetUp: WifiHotspot enabled";
    }
    if (advertising_options_.allowed.wifi_direct) {
      LOG(INFO) << "SetUp: WifiDirect enabled";
    }
    if (advertising_options_.allowed.web_rtc) {
      LOG(INFO) << "SetUp: WebRTC enabled";
    }
    if (advertising_options_.allowed.awdl) {
      LOG(INFO) << "SetUp: Awdl enabled";
    }
    LOG(INFO) << "SetUp: end";
  }

  ClientProxy client_a_;
  ClientProxy client_b_;
  std::string service_id_{"service"};
  ConnectionOptions connection_options_{
      {
          Strategy::kP2pPointToPoint,
          std::get<0>(GetParam()),
      },
      false,  // auto_upgrade_bandwidth
      true,   // enforce_topology_constraints
  };
  AdvertisingOptions advertising_options_{
      {
          Strategy::kP2pPointToPoint,
          std::get<0>(GetParam()),
      },
      false,  // auto_upgrade_bandwidth
      true,   // enforce_topology_constraints
  };
  DiscoveryOptions discovery_options_{
      {
          Strategy::kP2pPointToPoint,
          std::get<0>(GetParam()),
      },
  };
  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(P2pPointToPointPcpHandlerTest, CanConstructOne) {
  env_.Start();
  Mediums mediums;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(mediums, em, ecm, {}, {});
  InjectedBluetoothDeviceStore ibds;
  P2pPointToPointPcpHandler handler(mediums, em, ecm, bwu, ibds);
  env_.Stop();
}

TEST_P(P2pPointToPointPcpHandlerTest, CanConnect) {
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
  P2pPointToPointPcpHandler handler_a(mediums_a, em_a, ecm_a, bwu_a, ibds_a);
  P2pPointToPointPcpHandler handler_b(mediums_b, em_b, ecm_b, bwu_b, ibds_b);
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
                            LOG(INFO)
                                << "StartAdvertising: initiated_cb called";
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
                          LOG(INFO) << "Device discovered: id=" << endpoint_id
                                    << ", endpoint_info="
                                    << endpoint_info.AsStringView();
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
  constexpr char kIp4Bytes[] = {(char)192, (char)168, (char)1, (char)37, 0};

  connection_options_.connection_info.supports_5_ghz = true;
  connection_options_.connection_info.bssid = kBssid;
  connection_options_.connection_info.ap_frequency = kFreq;
  connection_options_.connection_info.ip_address.resize(4);
  connection_options_.connection_info.ip_address = kIp4Bytes;

  client_b_.AddCancellationFlag(discovered.endpoint_id);
  handler_b.RequestConnection(
      &client_b_, discovered.endpoint_id,
      {.endpoint_info = discovered.endpoint_info,
       .listener =
           {
               .initiated_cb =
                   [&connect_latch](const std::string& endpoint_id,
                                    const ConnectionResponseInfo& info) {
                     LOG(INFO) << "RequestConnection: initiated_cb called";
                     connect_latch.CountDown();
                   },
           }},
      connection_options_);
  std::string client_b_local_endpoint = client_b_.GetLocalEndpointId();

  EXPECT_TRUE(connect_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_TRUE(client_b_.Is5GHzSupported(discovered.endpoint_id));
  EXPECT_EQ(client_b_.GetBssid(discovered.endpoint_id), kBssid);
  EXPECT_EQ(client_b_.GetApFrequency(discovered.endpoint_id), kFreq);
  EXPECT_EQ(client_b_.GetIPAddress(discovered.endpoint_id),
            std::string(kIp4Bytes));
  // When connection is established, EndpointManager will setup KeepAliveManager
  // loop. When it fails, the connection will be dismantled. Since this a unit
  // test, KeepAliveManager won't be really up. The disconnection may happen
  // before the following check, which cause the check fail. So we check the
  // connection status first.
  if (client_b_.IsConnectedToEndpoint(discovered.endpoint_id)) {
    EXPECT_EQ(client_a_.Is5GHzSupported(client_b_local_endpoint),
              mediums_b.GetWifi().GetCapability().supports_5_ghz);
    EXPECT_EQ(client_a_.GetBssid(client_b_local_endpoint),
              mediums_b.GetWifi().GetInformation().bssid);
    EXPECT_EQ(client_a_.GetApFrequency(client_b_local_endpoint),
              mediums_b.GetWifi().GetInformation().ap_frequency);
    EXPECT_EQ(client_a_.GetIPAddress(client_b_local_endpoint),
              mediums_b.GetWifi().GetInformation().ip_address_4_bytes);
  }

  handler_a.StopAdvertising(&client_a_);
  handler_b.StopDiscovery(&client_b_);
  bwu_a.Shutdown();
  bwu_b.Shutdown();
  handler_a.DisconnectFromEndpointManager();
  handler_b.DisconnectFromEndpointManager();
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedPcpHandlerTest,
                         P2pPointToPointPcpHandlerTest,
                         ::testing::Combine(::testing::ValuesIn(kTestCases)));

}  // namespace
}  // namespace connections
}  // namespace nearby
