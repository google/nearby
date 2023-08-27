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

#include "connections/implementation/p2p_cluster_pcp_handler.h"

#include <memory>
#include <string>
#include <tuple>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "connections/advertising_options.h"
#include "connections/implementation/bluetooth_device_name.h"
#include "connections/implementation/bwu_manager.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/injected_bluetooth_device_store.h"
#include "connections/medium_selector.h"
#include "connections/v3/connection_listening_options.h"
#include "internal/flags/nearby_flags.h"
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

// Combines the bool `kEnableBleV2` as param testing but should revert it back
// if ble_v2 is done and ble will be replaced by ble_v2.
class P2pClusterPcpHandlerTest
    : public testing::TestWithParam<std::tuple<BooleanMediumSelector, bool>> {
 protected:
  void SetUp() override {
    NEARBY_LOG(INFO, "SetUp: begin");
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::kEnableBleV2,
        std::get<1>(GetParam()));
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

TEST_P(P2pClusterPcpHandlerTest, CanUpdateAdvertisingOptions) {
  bool ble_v2_enabled = std::get<1>(GetParam());
  if (!ble_v2_enabled) {
    // Just don't run the test if ble_v2 is disabled.
    return;
  }
  env_.Start();
  std::string endpoint_name{"endpoint_name"};
  Mediums mediums_a;
  EndpointChannelManager ecm_a;
  EndpointManager em_a(&ecm_a);
  BwuManager bwu_a(mediums_a, em_a, ecm_a, {}, {});
  InjectedBluetoothDeviceStore ibds_a;
  P2pClusterPcpHandler handler_a(&mediums_a, &em_a, &ecm_a, &bwu_a, ibds_a);
  // Reset medium states.
  mediums_a.GetBluetoothClassic().TurnOffDiscoverability();
  ASSERT_FALSE(mediums_a.GetBluetoothClassic().TurnOffDiscoverability());
  mediums_a.GetBluetoothClassic().StopAcceptingConnections(service_id_);
  ASSERT_FALSE(
      mediums_a.GetBluetoothClassic().IsAcceptingConnections(service_id_));
  mediums_a.GetWifiLan().StopAcceptingConnections(service_id_);
  ASSERT_FALSE(mediums_a.GetWifiLan().IsAcceptingConnections(service_id_));
  mediums_a.GetWifiLan().StopAdvertising(service_id_);
  ASSERT_FALSE(mediums_a.GetWifiLan().IsAdvertising(service_id_));
  mediums_a.GetBleV2().StopAcceptingConnections(service_id_);
  ASSERT_FALSE(mediums_a.GetBleV2().IsAcceptingConnections(service_id_));
  mediums_a.GetBleV2().StopAdvertising(service_id_);
  ASSERT_FALSE(mediums_a.GetBleV2().IsAdvertising(service_id_));
  EXPECT_EQ(
      handler_a.StartAdvertising(&client_a_, service_id_, advertising_options_,
                                 {.endpoint_info = ByteArray{endpoint_name}}),
      Status{Status::kSuccess});
  BooleanMediumSelector enabled = advertising_options_.allowed;
  EXPECT_EQ(enabled.ble, mediums_a.GetBleV2().IsAdvertising(service_id_));
  EXPECT_EQ(enabled.wifi_lan,
            mediums_a.GetWifiLan().IsAdvertising(service_id_));
  EXPECT_EQ(
      enabled.bluetooth || enabled.ble,
      mediums_a.GetBluetoothClassic().IsAcceptingConnections(service_id_));
  EXPECT_EQ(enabled.bluetooth,
            mediums_a.GetBluetoothClassic().TurnOffDiscoverability());
  // Turn discoverability back on
  mediums_a.GetBluetoothClassic().TurnOnDiscoverability(service_id_);
  AdvertisingOptions new_options{
      {
          Strategy::kP2pCluster,
          enabled,
      },
      false,  // auto_upgrade_bandwidth
      false,  // enforce_topology_constraints
      true,   // low_power
  };
  EXPECT_EQ(
      handler_a.UpdateAdvertisingOptions(&client_a_, service_id_, new_options),
      Status{Status::kSuccess});
  if (ble_v2_enabled) {
    EXPECT_EQ(enabled.ble, mediums_a.GetBleV2().IsAdvertising(service_id_));
  } else {
    EXPECT_EQ(enabled.ble, mediums_a.GetBle().IsAdvertising(service_id_));
  }
  EXPECT_FALSE(mediums_a.GetWifiLan().IsAdvertising(service_id_));
  EXPECT_FALSE(mediums_a.GetBluetoothClassic().TurnOffDiscoverability());
  EXPECT_FALSE(
      mediums_a.GetBluetoothClassic().IsAcceptingConnections(service_id_));
  handler_a.StopAdvertising(&client_a_);
  env_.Stop();
}

TEST_P(P2pClusterPcpHandlerTest, CanUpdateAdvertisingOptionsNoLowPower) {
  bool ble_v2_enabled = std::get<1>(GetParam());
  if (!ble_v2_enabled) {
    // Just don't run the test if ble_v2 is disabled.
    return;
  }
  env_.Start();
  std::string endpoint_name{"endpoint_name"};
  Mediums mediums_a;
  EndpointChannelManager ecm_a;
  EndpointManager em_a(&ecm_a);
  BwuManager bwu_a(mediums_a, em_a, ecm_a, {}, {});
  InjectedBluetoothDeviceStore ibds_a;
  P2pClusterPcpHandler handler_a(&mediums_a, &em_a, &ecm_a, &bwu_a, ibds_a);
  BooleanMediumSelector enabled = advertising_options_.allowed;
  // Reset medium states.
  mediums_a.GetBluetoothClassic().TurnOffDiscoverability();
  ASSERT_FALSE(mediums_a.GetBluetoothClassic().TurnOffDiscoverability());
  mediums_a.GetBluetoothClassic().StopAcceptingConnections(service_id_);
  ASSERT_FALSE(
      mediums_a.GetBluetoothClassic().IsAcceptingConnections(service_id_));
  mediums_a.GetWifiLan().StopAcceptingConnections(service_id_);
  ASSERT_FALSE(mediums_a.GetWifiLan().IsAcceptingConnections(service_id_));
  mediums_a.GetWifiLan().StopAdvertising(service_id_);
  ASSERT_FALSE(mediums_a.GetWifiLan().IsAdvertising(service_id_));
  mediums_a.GetBleV2().StopAcceptingConnections(service_id_);
  ASSERT_FALSE(mediums_a.GetBleV2().IsAcceptingConnections(service_id_));
  mediums_a.GetBleV2().StopAdvertising(service_id_);
  ASSERT_FALSE(mediums_a.GetBleV2().IsAdvertising(service_id_));
  AdvertisingOptions old_options{
      {
          Strategy::kP2pCluster,
          enabled,
      },
      false,  // auto_upgrade_bandwidth
      false,  // enforce_topology_constraints
      true,   // low_power
      false,  // enable_bluetooth_listening
  };
  EXPECT_EQ(
      handler_a.StartAdvertising(&client_a_, service_id_, old_options,
                                 {.endpoint_info = ByteArray{endpoint_name}}),
      Status{Status::kSuccess});
  EXPECT_EQ(enabled.ble, mediums_a.GetBleV2().IsAdvertising(service_id_));
  EXPECT_EQ(enabled.wifi_lan,
            mediums_a.GetWifiLan().IsAdvertising(service_id_));
  EXPECT_EQ(
      enabled.bluetooth,
      mediums_a.GetBluetoothClassic().IsAcceptingConnections(service_id_));
  EXPECT_EQ(enabled.bluetooth,
            mediums_a.GetBluetoothClassic().TurnOffDiscoverability());
  EXPECT_EQ(handler_a.UpdateAdvertisingOptions(&client_a_, service_id_,
                                               advertising_options_),
            Status{Status::kSuccess});
  if (ble_v2_enabled) {
    EXPECT_EQ(enabled.ble, mediums_a.GetBleV2().IsAdvertising(service_id_));
  } else {
    EXPECT_EQ(enabled.ble, mediums_a.GetBle().IsAdvertising(service_id_));
  }
  EXPECT_EQ(enabled.wifi_lan,
            mediums_a.GetWifiLan().IsAdvertising(service_id_));
  EXPECT_EQ(enabled.bluetooth,
            mediums_a.GetBluetoothClassic().TurnOffDiscoverability());
  EXPECT_EQ(
      enabled.bluetooth || enabled.ble,
      mediums_a.GetBluetoothClassic().IsAcceptingConnections(service_id_));
  handler_a.StopAdvertising(&client_a_);
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

TEST_P(P2pClusterPcpHandlerTest, CanBluetoothDiscoverChangeName) {
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
  // For the first time we find the device with the old name.
  CountDownLatch first_found_latch(1);
  // For the second time we "find" the device with the new name.
  CountDownLatch second_found_latch(1);
  // For when the name is changed.
  CountDownLatch lost_latch(1);
  AdvertisingOptions advertising_options = {
      {
          Strategy::kP2pCluster,
          BooleanMediumSelector{
              .bluetooth = true,
          },
      },
  };

  DiscoveryOptions discovery_options = {
      {
          Strategy::kP2pCluster,
          BooleanMediumSelector{
              .bluetooth = true,
          },
      },
  };
  bool first = false;
  EXPECT_EQ(
      handler_a.StartAdvertising(&client_a_, service_id_, advertising_options,
                                 {.endpoint_info = ByteArray{endpoint_name}}),
      Status{Status::kSuccess});
  EXPECT_EQ(handler_b.StartDiscovery(
                &client_b_, service_id_, discovery_options,
                {
                    .endpoint_found_cb =
                        [&](const std::string& endpoint_id,
                            const ByteArray& endpoint_info,
                            const std::string& service_id) {
                          NEARBY_LOG(INFO, "Device discovered: id=%s",
                                     endpoint_id.c_str());
                          if (!first) {
                            first_found_latch.CountDown();
                            first = true;
                          } else {
                            second_found_latch.CountDown();
                          }
                        },
                    .endpoint_lost_cb =
                        [&](const std::string& id) {
                          NEARBY_LOG(INFO, "Device lost: id=%s", id.c_str());
                          lost_latch.CountDown();
                        },
                }),
            Status{Status::kSuccess});
  ASSERT_TRUE(mediums_a.GetBluetoothRadio().IsAdapterValid());
  BluetoothDeviceName name(
      mediums_a.GetBluetoothRadio().GetBluetoothAdapter().GetName());
  BluetoothDeviceName new_name(name.GetVersion(), name.GetPcp(),
                               name.GetEndpointId(), name.GetServiceIdHash(),
                               ByteArray("BT Device A"), name.GetUwbAddress(),
                               name.GetWebRtcState());
  EXPECT_TRUE(first_found_latch.Await().Ok());
  mediums_a.GetBluetoothRadio().GetBluetoothAdapter().SetName(
      std::string(new_name));
  EXPECT_TRUE(second_found_latch.Await().Ok());
  EXPECT_TRUE(lost_latch.Await().Ok());
  handler_b.StopDiscovery(&client_b_);
  env_.Stop();
}

TEST_P(P2pClusterPcpHandlerTest, CanUpdateDiscoveryOptions) {
  env_.Start();
  std::string endpoint_name{"endpoint_name"};
  Mediums mediums_a;
  EndpointChannelManager ecm_a;
  EndpointManager em_a(&ecm_a);
  BwuManager bwu_a(mediums_a, em_a, ecm_a, {}, {});
  InjectedBluetoothDeviceStore ibds_a;
  P2pClusterPcpHandler handler_a(&mediums_a, &em_a, &ecm_a, &bwu_a, ibds_a);
  EXPECT_EQ(
      handler_a.StartDiscovery(&client_a_, service_id_, discovery_options_, {}),
      Status{Status::kSuccess});
  BooleanMediumSelector enabled = std::get<0>(GetParam());
  bool ble_v2_enabled = std::get<1>(GetParam());
  if (ble_v2_enabled) {
    EXPECT_EQ(enabled.ble, mediums_a.GetBleV2().IsScanning(service_id_));
  } else {
    EXPECT_EQ(enabled.ble, mediums_a.GetBle().IsScanning(service_id_));
  }
  EXPECT_EQ(enabled.wifi_lan,
            mediums_a.GetWifiLan().IsDiscovering(service_id_));
  DiscoveryOptions new_options{
      {
          Strategy::kP2pCluster,
          std::get<0>(GetParam()),
      },
      false,  // auto_upgrade_bandwidth
      false,  // enforce_topology_constraints
      false,  // is_out_of_band_connection
      "",     // fast_advertisement_service_uuid
      true,   // low_power
  };
  EXPECT_EQ(
      handler_a.UpdateDiscoveryOptions(&client_a_, service_id_, new_options),
      Status{Status::kSuccess});
  if (ble_v2_enabled) {
    EXPECT_EQ(enabled.ble, mediums_a.GetBleV2().IsScanning(service_id_));
  } else {
    EXPECT_EQ(enabled.ble, mediums_a.GetBle().IsScanning(service_id_));
  }
  EXPECT_FALSE(mediums_a.GetWifiLan().IsDiscovering(service_id_));
  handler_a.StopDiscovery(&client_a_);
  env_.Stop();
}

TEST_P(P2pClusterPcpHandlerTest, CanUpdateDiscoveryOptionsNoLowPower) {
  env_.Start();
  std::string endpoint_name{"endpoint_name"};
  Mediums mediums_a;
  EndpointChannelManager ecm_a;
  EndpointManager em_a(&ecm_a);
  BwuManager bwu_a(mediums_a, em_a, ecm_a, {}, {});
  InjectedBluetoothDeviceStore ibds_a;
  P2pClusterPcpHandler handler_a(&mediums_a, &em_a, &ecm_a, &bwu_a, ibds_a);
  BooleanMediumSelector old_enabled = discovery_options_.allowed;
  BooleanMediumSelector new_enabled = old_enabled;
  if (!new_enabled.ble) {
    new_enabled.ble = true;
  }
  // Reset medium states.
  mediums_a.GetBluetoothClassic().TurnOffDiscoverability();
  ASSERT_FALSE(mediums_a.GetBluetoothClassic().TurnOffDiscoverability());
  mediums_a.GetWifiLan().StopDiscovery(service_id_);
  ASSERT_FALSE(mediums_a.GetWifiLan().IsDiscovering(service_id_));
  if (std::get<1>(GetParam())) {
    mediums_a.GetBleV2().StopScanning(service_id_);
    ASSERT_FALSE(mediums_a.GetBleV2().IsScanning(service_id_));
  } else {
    mediums_a.GetBle().StopScanning(service_id_);
    ASSERT_FALSE(mediums_a.GetBle().IsScanning(service_id_));
  }
  DiscoveryOptions old_options = discovery_options_;
  old_options.low_power = true;
  old_options.allowed = old_enabled;
  DiscoveryOptions new_options = discovery_options_;
  new_options.allowed = new_enabled;
  // Start discovery
  EXPECT_EQ(handler_a.StartDiscovery(&client_a_, service_id_, old_options, {}),
            Status{Status::kSuccess});
  if (std::get<1>(GetParam())) {
    EXPECT_EQ(old_enabled.ble, mediums_a.GetBleV2().IsScanning(service_id_));
  } else {
    EXPECT_EQ(old_enabled.ble, mediums_a.GetBle().IsScanning(service_id_));
  }
  EXPECT_EQ(old_enabled.wifi_lan,
            mediums_a.GetWifiLan().IsDiscovering(service_id_));
  EXPECT_EQ(old_enabled.bluetooth,
            mediums_a.GetBluetoothClassic().StopDiscovery());
  NEARBY_LOGS(INFO) << "started discovery";
  // Update discovery options
  EXPECT_TRUE(
      handler_a.UpdateDiscoveryOptions(&client_a_, service_id_, new_options)
          .Ok());
  NEARBY_LOGS(INFO) << "updated discovery options";
  if (std::get<1>(GetParam())) {
    EXPECT_EQ(new_enabled.ble, mediums_a.GetBleV2().IsScanning(service_id_));
  } else {
    EXPECT_EQ(new_enabled.ble, mediums_a.GetBle().IsScanning(service_id_));
  }
  EXPECT_EQ(new_enabled.wifi_lan,
            mediums_a.GetWifiLan().IsDiscovering(service_id_));
  EXPECT_EQ(new_enabled.bluetooth,
            mediums_a.GetBluetoothClassic().StopDiscovery());
  handler_a.StopDiscovery(&client_a_);
  env_.Stop();
}

TEST_P(P2pClusterPcpHandlerTest, UpdateDiscoveryOptionsSkipMediumRestart) {
  env_.Start();
  std::string endpoint_name{"endpoint_name"};
  Mediums mediums_a;
  EndpointChannelManager ecm_a;
  EndpointManager em_a(&ecm_a);
  BwuManager bwu_a(mediums_a, em_a, ecm_a, {}, {});
  InjectedBluetoothDeviceStore ibds_a;
  P2pClusterPcpHandler handler_a(&mediums_a, &em_a, &ecm_a, &bwu_a, ibds_a);
  BooleanMediumSelector enabled = discovery_options_.allowed;
  // Reset medium states.
  mediums_a.GetBluetoothClassic().TurnOffDiscoverability();
  ASSERT_FALSE(mediums_a.GetBluetoothClassic().TurnOffDiscoverability());
  mediums_a.GetWifiLan().StopDiscovery(service_id_);
  ASSERT_FALSE(mediums_a.GetWifiLan().IsDiscovering(service_id_));
  if (std::get<1>(GetParam())) {
    mediums_a.GetBleV2().StopScanning(service_id_);
    ASSERT_FALSE(mediums_a.GetBleV2().IsScanning(service_id_));
  } else {
    mediums_a.GetBle().StopScanning(service_id_);
    ASSERT_FALSE(mediums_a.GetBle().IsScanning(service_id_));
  }
  // Start discovery
  EXPECT_EQ(
      handler_a.StartDiscovery(&client_a_, service_id_, discovery_options_, {}),
      Status{Status::kSuccess});
  if (std::get<1>(GetParam())) {
    EXPECT_EQ(enabled.ble, mediums_a.GetBleV2().IsScanning(service_id_));
  } else {
    EXPECT_EQ(enabled.ble, mediums_a.GetBle().IsScanning(service_id_));
  }
  EXPECT_EQ(enabled.wifi_lan,
            mediums_a.GetWifiLan().IsDiscovering(service_id_));
  EXPECT_EQ(enabled.bluetooth, mediums_a.GetBluetoothClassic().StopDiscovery());
  // Update discovery options
  auto result = handler_a.UpdateDiscoveryOptions(&client_a_, service_id_,
                                                 discovery_options_);
  EXPECT_TRUE(result.Ok());
  NEARBY_LOGS(INFO) << "updated discovery options";
  if (std::get<1>(GetParam())) {
    EXPECT_EQ(enabled.ble, mediums_a.GetBleV2().IsScanning(service_id_));
  } else {
    EXPECT_EQ(enabled.ble, mediums_a.GetBle().IsScanning(service_id_));
  }
  EXPECT_EQ(enabled.wifi_lan,
            mediums_a.GetWifiLan().IsDiscovering(service_id_));
  // We didn't restart the medium.
  EXPECT_FALSE(mediums_a.GetBluetoothClassic().StopDiscovery());
  handler_a.StopDiscovery(&client_a_);
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
  constexpr char kIp4Bytes[] = {(char)192, (char)168, (char)1, (char)37, 0};

  connection_options_.connection_info.supports_5_ghz = true;
  connection_options_.connection_info.bssid = kBssid;
  connection_options_.connection_info.ap_frequency = kFreq;
  connection_options_.connection_info.ip_address.resize(4);
  connection_options_.connection_info.ip_address = std::string(kIp4Bytes);

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
  std::string client_b_local_endpoint = client_b_.GetLocalEndpointId();

  EXPECT_TRUE(connect_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_TRUE(client_b_.Is5GHzSupported(discovered.endpoint_id));
  EXPECT_EQ(client_b_.GetBssid(discovered.endpoint_id), kBssid);
  EXPECT_EQ(client_b_.GetApFrequency(discovered.endpoint_id), kFreq);
  EXPECT_EQ(client_b_.GetIPAddress(discovered.endpoint_id),
            std::string(kIp4Bytes));
  EXPECT_EQ(client_a_.Is5GHzSupported(client_b_local_endpoint),
            mediums_b.GetWifi().GetCapability().supports_5_ghz);
  EXPECT_EQ(client_a_.GetBssid(client_b_local_endpoint),
            mediums_b.GetWifi().GetInformation().bssid);
  EXPECT_EQ(client_a_.GetApFrequency(client_b_local_endpoint),
            mediums_b.GetWifi().GetInformation().ap_frequency);
  EXPECT_EQ(client_a_.GetIPAddress(client_b_local_endpoint),
            mediums_b.GetWifi().GetInformation().ip_address_4_bytes);

  handler_b.StopDiscovery(&client_b_);
  bwu_a.Shutdown();
  bwu_b.Shutdown();
  env_.Stop();
}

TEST_P(P2pClusterPcpHandlerTest, CanStartListeningForIncomingConnections) {
  env_.Start();
  std::string endpoint_name_a{"endpoint_name"};
  Mediums mediums_a;
  BluetoothRadio& radio_a = mediums_a.GetBluetoothRadio();
  radio_a.GetBluetoothAdapter().SetName("BT Device A");
  EndpointChannelManager ecm_a;
  EndpointManager em_a(&ecm_a);
  BwuManager bwu_a(mediums_a, em_a, ecm_a, {},
                   {.allow_upgrade_to = {.bluetooth = true}});
  InjectedBluetoothDeviceStore ibds_a;
  P2pClusterPcpHandler handler_a(&mediums_a, &em_a, &ecm_a, &bwu_a, ibds_a);
  v3::ConnectionListeningOptions v3_options{
      .strategy = Strategy::kP2pCluster,
      .enable_ble_listening = true,
      .enable_bluetooth_listening = true,
      .enable_wlan_listening = true,
  };
  // make sure mediums are not accepting before calling handler.
  ASSERT_FALSE(
      mediums_a.GetBluetoothClassic().IsAcceptingConnections(service_id_));
  ASSERT_FALSE(mediums_a.GetWifiLan().IsAcceptingConnections(service_id_));
  if (std::get<1>(GetParam())) {
    ASSERT_FALSE(mediums_a.GetBleV2().IsAcceptingConnections(service_id_));
  } else {
    ASSERT_FALSE(mediums_a.GetBle().IsAcceptingConnections(service_id_));
  }
  // call handler.
  auto result = handler_a.StartListeningForIncomingConnections(
      &client_a_, service_id_, v3_options, {});
  // now check to make sure we are in fact accepting connections.
  EXPECT_TRUE(
      mediums_a.GetBluetoothClassic().IsAcceptingConnections(service_id_));
  EXPECT_TRUE(mediums_a.GetWifiLan().IsAcceptingConnections(service_id_));
  if (std::get<1>(GetParam())) {
    EXPECT_TRUE(mediums_a.GetBleV2().IsAcceptingConnections(service_id_));
  } else {
    EXPECT_TRUE(mediums_a.GetBle().IsAcceptingConnections(service_id_));
  }
  EXPECT_EQ(result.second.size(), 3);
  ASSERT_TRUE(client_a_.IsListeningForIncomingConnections());
  EXPECT_EQ(client_a_.GetListeningForIncomingConnectionsServiceId(),
            service_id_);
  EXPECT_EQ(client_a_.GetListeningOptions().enable_ble_listening,
            v3_options.enable_ble_listening);
  EXPECT_EQ(client_a_.GetListeningOptions().enable_bluetooth_listening,
            v3_options.enable_bluetooth_listening);
  EXPECT_EQ(client_a_.GetListeningOptions().enable_wlan_listening,
            v3_options.enable_wlan_listening);
  EXPECT_EQ(client_a_.GetListeningOptions().strategy, v3_options.strategy);
  env_.Stop();
}

TEST_P(P2pClusterPcpHandlerTest, CanStopListeningForIncomingConnections) {
  env_.Start();
  std::string endpoint_name_a{"endpoint_name"};
  Mediums mediums_a;
  BluetoothRadio& radio_a = mediums_a.GetBluetoothRadio();
  radio_a.GetBluetoothAdapter().SetName("BT Device A");
  EndpointChannelManager ecm_a;
  EndpointManager em_a(&ecm_a);
  BwuManager bwu_a(mediums_a, em_a, ecm_a, {},
                   {.allow_upgrade_to = {.bluetooth = true}});
  InjectedBluetoothDeviceStore ibds_a;
  P2pClusterPcpHandler handler_a(&mediums_a, &em_a, &ecm_a, &bwu_a, ibds_a);
  v3::ConnectionListeningOptions v3_options{
      .strategy = Strategy::kP2pCluster,
      .enable_ble_listening = true,
      .enable_bluetooth_listening = true,
      .enable_wlan_listening = true,
  };
  // make sure mediums are not accepting before calling handler.
  ASSERT_FALSE(
      mediums_a.GetBluetoothClassic().IsAcceptingConnections(service_id_));
  ASSERT_FALSE(mediums_a.GetWifiLan().IsAcceptingConnections(service_id_));
  if (std::get<1>(GetParam())) {
    ASSERT_FALSE(mediums_a.GetBleV2().IsAcceptingConnections(service_id_));
  } else {
    ASSERT_FALSE(mediums_a.GetBle().IsAcceptingConnections(service_id_));
  }
  // call handler.
  auto result = handler_a.StartListeningForIncomingConnections(
      &client_a_, service_id_, v3_options, {});
  // now check to make sure we are in fact accepting connections.
  ASSERT_TRUE(
      mediums_a.GetBluetoothClassic().IsAcceptingConnections(service_id_));
  ASSERT_TRUE(mediums_a.GetWifiLan().IsAcceptingConnections(service_id_));
  if (std::get<1>(GetParam())) {
    ASSERT_TRUE(mediums_a.GetBleV2().IsAcceptingConnections(service_id_));
  } else {
    ASSERT_TRUE(mediums_a.GetBle().IsAcceptingConnections(service_id_));
  }
  // stop.
  handler_a.StopListeningForIncomingConnections(&client_a_);
  EXPECT_FALSE(
      mediums_a.GetBluetoothClassic().IsAcceptingConnections(service_id_));
  EXPECT_FALSE(mediums_a.GetWifiLan().IsAcceptingConnections(service_id_));
  if (std::get<1>(GetParam())) {
    EXPECT_FALSE(mediums_a.GetBleV2().IsAcceptingConnections(service_id_));
  } else {
    EXPECT_FALSE(mediums_a.GetBle().IsAcceptingConnections(service_id_));
  }
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedPcpHandlerTest, P2pClusterPcpHandlerTest,
                         ::testing::Combine(::testing::ValuesIn(kTestCases),
                                            ::testing::Bool()));

}  // namespace
}  // namespace connections
}  // namespace nearby
