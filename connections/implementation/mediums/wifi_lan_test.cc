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

#include "connections/implementation/mediums/wifi_lan.h"

#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/wifi_lan.h"
#include "internal/platform/base64_utils.h"

namespace nearby {
namespace connections {
namespace {

using FeatureFlags = FeatureFlags::Flags;

constexpr FeatureFlags kTestCases[] = {
    FeatureFlags{
        .enable_cancellation_flag = true,
    },
    FeatureFlags{
        .enable_cancellation_flag = false,
    },
};

constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kServiceInfoName{"ServiceInfoName"};
constexpr absl::string_view kEndpointName{"EndpointName"};
constexpr absl::string_view kEndpointInfoKey{"n"};

class WifiLanTest : public ::testing::TestWithParam<FeatureFlags> {
 protected:
  using DiscoveredServiceCallback = WifiLanMedium::DiscoveredServiceCallback;

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(WifiLanTest, CanConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  WifiLan wifi_lan_client;
  WifiLan wifi_lan_server;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  CountDownLatch discovered_latch(1);
  CountDownLatch accept_latch(1);

  WifiLanSocket socket_for_server;
  EXPECT_TRUE(wifi_lan_server.StartAcceptingConnections(
      service_id, [&](const std::string& service_id, WifiLanSocket socket) {
        socket_for_server = std::move(socket);
        accept_latch.CountDown();
      }));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  wifi_lan_server.StartAdvertising(service_id, nsd_service_info);

  NsdServiceInfo discovered_service_info;
  wifi_lan_client.StartDiscovery(
      service_id,
      {
          .service_discovered_cb =
              [&discovered_latch, &discovered_service_info](
                  NsdServiceInfo service_info, const std::string& service_id) {
                NEARBY_LOGS(INFO)
                    << "Discovered service_info=" << &service_info;
                discovered_service_info = service_info;
                discovered_latch.CountDown();
              },
      });
  discovered_latch.Await(kWaitDuration).result();
  ASSERT_TRUE(discovered_service_info.IsValid());

  CancellationFlag flag;
  WifiLanSocket socket_for_client =
      wifi_lan_client.Connect(service_id, discovered_service_info, &flag);
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(wifi_lan_server.StopAcceptingConnections(service_id));
  EXPECT_TRUE(wifi_lan_server.StopAdvertising(service_id));
  EXPECT_TRUE(socket_for_server.IsValid());
  EXPECT_TRUE(socket_for_client.IsValid());
  env_.Stop();
}

TEST_P(WifiLanTest, CanConnectWithMultiplex) {
  bool is_multiplex_enabled = NearbyFlags::GetInstance().GetBoolFlag(
      config_package_nearby::nearby_connections_feature::kEnableMultiplex);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::kEnableMultiplex,
      true);
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  WifiLan wifi_lan_client;
  WifiLan wifi_lan_server;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  CountDownLatch discovered_latch(1);
  CountDownLatch accept_latch(1);

  WifiLanSocket socket_for_server;
  EXPECT_TRUE(wifi_lan_server.StartAcceptingConnections(
      service_id, [&](const std::string& service_id, WifiLanSocket socket) {
        socket_for_server = std::move(socket);
        accept_latch.CountDown();
      }));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  wifi_lan_server.StartAdvertising(service_id, nsd_service_info);

  WifiLanSocket socket_for_client;
  SingleThreadExecutor client_executor;
  client_executor.Execute([&]() {
  NsdServiceInfo discovered_service_info;
  wifi_lan_client.StartDiscovery(
      service_id,
      {
          .service_discovered_cb =
              [&discovered_latch, &discovered_service_info](
                  NsdServiceInfo service_info, const std::string& service_id) {
                NEARBY_LOGS(INFO)
                    << "Discovered service_info=" << &service_info;
                discovered_service_info = service_info;
                discovered_latch.CountDown();
              },
      });
  discovered_latch.Await(kWaitDuration).result();
  ASSERT_TRUE(discovered_service_info.IsValid());

  CancellationFlag flag;
  socket_for_client =
      wifi_lan_client.Connect(service_id, discovered_service_info, &flag);
    Base64Utils::WriteInt(&socket_for_client.GetOutputStream(), 4);
  });
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(wifi_lan_server.StopAcceptingConnections(service_id));
  EXPECT_TRUE(wifi_lan_server.StopAdvertising(service_id));
  EXPECT_TRUE(socket_for_server.IsValid());
  EXPECT_TRUE(socket_for_client.IsValid());
  env_.Stop();
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::kEnableMultiplex,
      is_multiplex_enabled);
}

TEST_P(WifiLanTest, CanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  WifiLan wifi_lan_client;
  WifiLan wifi_lan_server;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  CountDownLatch discovered_latch(1);
  CountDownLatch accept_latch(1);

  WifiLanSocket socket_for_server;
  EXPECT_TRUE(wifi_lan_server.StartAcceptingConnections(
      service_id, [&](const std::string& service_id, WifiLanSocket socket) {
        socket_for_server = std::move(socket);
        accept_latch.CountDown();
      }));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  wifi_lan_server.StartAdvertising(service_id, nsd_service_info);

  NsdServiceInfo discovered_service_info;
  wifi_lan_client.StartDiscovery(
      service_id,
      {
          .service_discovered_cb =
              [&discovered_latch, &discovered_service_info](
                  NsdServiceInfo service_info, const std::string& service_id) {
                NEARBY_LOGS(INFO)
                    << "Discovered service_info=" << &service_info;
                discovered_service_info = service_info;
                discovered_latch.CountDown();
              },
      });
  EXPECT_TRUE(discovered_latch.Await(kWaitDuration).result());
  ASSERT_TRUE(discovered_service_info.IsValid());

  CancellationFlag flag(true);
  WifiLanSocket socket_for_client =
      wifi_lan_client.Connect(service_id, discovered_service_info, &flag);
  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(wifi_lan_server.StopAcceptingConnections(service_id));
    EXPECT_TRUE(wifi_lan_server.StopAdvertising(service_id));
    EXPECT_TRUE(socket_for_server.IsValid());
    EXPECT_TRUE(socket_for_client.IsValid());
  } else {
    EXPECT_FALSE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(wifi_lan_server.StopAcceptingConnections(service_id));
    EXPECT_TRUE(wifi_lan_server.StopAdvertising(service_id));
    EXPECT_FALSE(socket_for_server.IsValid());
    EXPECT_FALSE(socket_for_client.IsValid());
  }
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedWifiLanTest, WifiLanTest,
                         ::testing::ValuesIn(kTestCases));

TEST_F(WifiLanTest, CanConstructValidObject) {
  env_.Start();
  WifiLan wifi_lan_a;
  WifiLan wifi_lan_b;
  std::string service_id(kServiceID);

  EXPECT_TRUE(wifi_lan_a.IsAvailable());
  EXPECT_TRUE(wifi_lan_b.IsAvailable());
  env_.Stop();
}

TEST_F(WifiLanTest, CanStartAdvertising) {
  env_.Start();
  WifiLan wifi_lan_a;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);

  EXPECT_TRUE(wifi_lan_a.StartAcceptingConnections(service_id, {}));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  EXPECT_TRUE(wifi_lan_a.StartAdvertising(service_id, nsd_service_info));
  EXPECT_TRUE(wifi_lan_a.StopAdvertising(service_id));
  env_.Stop();
}

TEST_F(WifiLanTest, CanStartMultipleAdvertising) {
  env_.Start();
  WifiLan wifi_lan_a;
  std::string service_id_1(kServiceID);
  std::string service_id_2("com.google.location.nearby.apps.test_1");
  std::string service_info_name_1(kServiceInfoName);
  std::string service_info_name_2("ServiceInfoName_1");
  std::string endpoint_info_name(kEndpointName);

  EXPECT_TRUE(wifi_lan_a.StartAcceptingConnections(service_id_1, {}));
  EXPECT_TRUE(wifi_lan_a.StartAcceptingConnections(service_id_2, {}));

  NsdServiceInfo nsd_service_info_1;
  nsd_service_info_1.SetServiceName(service_info_name_1);
  nsd_service_info_1.SetTxtRecord(std::string(kEndpointInfoKey),
                                  endpoint_info_name);
  NsdServiceInfo nsd_service_info_2;
  nsd_service_info_2.SetServiceName(service_info_name_2);
  nsd_service_info_2.SetTxtRecord(std::string(kEndpointInfoKey),
                                  endpoint_info_name);
  EXPECT_TRUE(wifi_lan_a.StartAdvertising(service_id_1, nsd_service_info_1));
  EXPECT_TRUE(wifi_lan_a.StartAdvertising(service_id_2, nsd_service_info_2));
  EXPECT_TRUE(wifi_lan_a.StopAdvertising(service_id_1));
  EXPECT_TRUE(wifi_lan_a.StopAdvertising(service_id_2));
  EXPECT_TRUE(wifi_lan_a.StopAcceptingConnections(service_id_1));
  EXPECT_TRUE(wifi_lan_a.StopAcceptingConnections(service_id_2));
  env_.Stop();
}

TEST_F(WifiLanTest, CanStartDiscovery) {
  env_.Start();
  WifiLan wifi_lan_a;
  std::string service_id(kServiceID);

  EXPECT_TRUE(
      wifi_lan_a.StartDiscovery(service_id, DiscoveredServiceCallback{}));
  EXPECT_TRUE(wifi_lan_a.StopDiscovery(service_id));
  env_.Stop();
}

TEST_F(WifiLanTest, CanStartMultipleDiscovery) {
  env_.Start();
  WifiLan wifi_lan_a;
  std::string service_id_1(kServiceID);
  std::string service_id_2("com.google.location.nearby.apps.test_1");

  EXPECT_TRUE(
      wifi_lan_a.StartDiscovery(service_id_1, DiscoveredServiceCallback{}));

  EXPECT_TRUE(
      wifi_lan_a.StartDiscovery(service_id_2, DiscoveredServiceCallback{}));
  EXPECT_TRUE(wifi_lan_a.StopDiscovery(service_id_1));
  EXPECT_TRUE(wifi_lan_a.StopDiscovery(service_id_2));
  env_.Stop();
}

TEST_F(WifiLanTest, CanAdvertiseThatOtherMediumDiscover) {
  env_.Start();
  WifiLan wifi_lan_a;
  WifiLan wifi_lan_b;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  CountDownLatch discovered_latch(1);
  CountDownLatch lost_latch(1);

  wifi_lan_b.StartDiscovery(
      service_id, DiscoveredServiceCallback{
                      .service_discovered_cb =
                          [&discovered_latch](NsdServiceInfo service_info,
                                              const std::string& service_id) {
                            discovered_latch.CountDown();
                          },
                      .service_lost_cb =
                          [&lost_latch](NsdServiceInfo service_info,
                                        const std::string& service_id) {
                            lost_latch.CountDown();
                          },
                  });

  EXPECT_TRUE(wifi_lan_a.StartAcceptingConnections(service_id, {}));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  EXPECT_TRUE(wifi_lan_a.StartAdvertising(service_id, nsd_service_info));
  EXPECT_TRUE(discovered_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(wifi_lan_a.StopAdvertising(service_id));
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(wifi_lan_b.StopDiscovery(service_id));
  env_.Stop();
}

TEST_F(WifiLanTest, CanDiscoverThatOtherMediumAdvertise) {
  env_.Start();
  WifiLan wifi_lan_a;
  WifiLan wifi_lan_b;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  CountDownLatch discovered_latch(1);
  CountDownLatch lost_latch(1);

  EXPECT_TRUE(wifi_lan_b.StartAcceptingConnections(service_id, {}));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  wifi_lan_b.StartAdvertising(service_id, nsd_service_info);

  EXPECT_TRUE(wifi_lan_a.StartDiscovery(
      service_id, DiscoveredServiceCallback{
                      .service_discovered_cb =
                          [&discovered_latch](NsdServiceInfo service_info,
                                              const std::string& service_id) {
                            discovered_latch.CountDown();
                          },
                      .service_lost_cb =
                          [&lost_latch](NsdServiceInfo service_info,
                                        const std::string& service_id) {
                            lost_latch.CountDown();
                          },
                  }));
  EXPECT_TRUE(discovered_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(wifi_lan_b.StopAdvertising(service_id));
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(wifi_lan_a.StopDiscovery(service_id));
  env_.Stop();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
