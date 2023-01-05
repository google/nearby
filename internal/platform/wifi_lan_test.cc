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

#include "internal/platform/wifi_lan.h"

#include <memory>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
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
constexpr absl::string_view kServiceId{"service_id"};
constexpr absl::string_view kServiceType{"_service.tcp_"};
constexpr absl::string_view kServiceInfoName{"Simulated service info name"};
constexpr absl::string_view kEndpointName{"Simulated endpoint name"};
constexpr absl::string_view kEndpointInfoKey{"n"};

class WifiLanMediumTest : public ::testing::TestWithParam<FeatureFlags> {
 protected:
  using DiscoveredServiceCallback = WifiLanMedium::DiscoveredServiceCallback;

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(WifiLanMediumTest, CanConnectToService) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  WifiLanMedium wifi_lan_a;
  WifiLanMedium wifi_lan_b;
  std::string service_id(kServiceId);
  std::string service_type(kServiceType);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  CountDownLatch discovered_latch(1);
  CountDownLatch lost_latch(1);

  WifiLanServerSocket server_socket = wifi_lan_b.ListenForService();
  EXPECT_TRUE(server_socket.IsValid());

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  nsd_service_info.SetServiceType(service_type);
  nsd_service_info.SetIPAddress(server_socket.GetIPAddress());
  nsd_service_info.SetPort(server_socket.GetPort());
  wifi_lan_b.StartAdvertising(nsd_service_info);

  NsdServiceInfo discovered_service_info;
  wifi_lan_a.StartDiscovery(
      service_id, service_type,
      DiscoveredServiceCallback{
          .service_discovered_cb =
              [&discovered_latch, &discovered_service_info](
                  NsdServiceInfo service_info,
                  const std::string& service_type) {
                discovered_service_info = service_info;
                discovered_latch.CountDown();
              },
          .service_lost_cb =
              [&lost_latch](NsdServiceInfo service_info,
                            const std::string& service_type) {
                lost_latch.CountDown();
              },
      });
  EXPECT_TRUE(discovered_latch.Await(absl::Milliseconds(1000)).result());
  WifiLanSocket socket_a;
  WifiLanSocket socket_b;
  EXPECT_FALSE(socket_a.IsValid());
  EXPECT_FALSE(socket_b.IsValid());
  {
    CancellationFlag flag;
    SingleThreadExecutor server_executor;
    SingleThreadExecutor client_executor;
    client_executor.Execute([&wifi_lan_a, &socket_a,
                             discovered_service_info = discovered_service_info,
                             service_type, &server_socket, &flag]() {
      socket_a = wifi_lan_a.ConnectToService(discovered_service_info, &flag);
      if (!socket_a.IsValid()) {
        server_socket.Close();
      }
    });
    server_executor.Execute([&socket_b, &server_socket]() {
      socket_b = server_socket.Accept();
      if (!socket_b.IsValid()) {
        server_socket.Close();
      }
    });
  }
  EXPECT_TRUE(socket_a.IsValid());
  EXPECT_TRUE(socket_b.IsValid());
  server_socket.Close();
  env_.Stop();
}

TEST_P(WifiLanMediumTest, CanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  WifiLanMedium wifi_lan_a;
  WifiLanMedium wifi_lan_b;
  std::string service_id(kServiceId);
  std::string service_type(kServiceType);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  CountDownLatch discovered_latch(1);
  CountDownLatch lost_latch(1);

  WifiLanServerSocket server_socket = wifi_lan_b.ListenForService();
  EXPECT_TRUE(server_socket.IsValid());

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  nsd_service_info.SetServiceType(service_type);
  nsd_service_info.SetIPAddress(server_socket.GetIPAddress());
  nsd_service_info.SetPort(server_socket.GetPort());
  wifi_lan_b.StartAdvertising(nsd_service_info);

  NsdServiceInfo discovered_service_info;
  wifi_lan_a.StartDiscovery(
      service_id, service_type,
      DiscoveredServiceCallback{
          .service_discovered_cb =
              [&discovered_latch, &discovered_service_info](
                  NsdServiceInfo service_info,
                  const std::string& service_type) {
                discovered_service_info = service_info;
                discovered_latch.CountDown();
              },
          .service_lost_cb =
              [&lost_latch](NsdServiceInfo service_info,
                            const std::string& service_type) {
                lost_latch.CountDown();
              },
      });
  EXPECT_TRUE(discovered_latch.Await(absl::Milliseconds(1000)).result());
  WifiLanSocket socket_a;
  WifiLanSocket socket_b;
  EXPECT_FALSE(socket_a.IsValid());
  EXPECT_FALSE(socket_b.IsValid());
  {
    CancellationFlag flag(true);
    SingleThreadExecutor server_executor;
    SingleThreadExecutor client_executor;
    client_executor.Execute([&wifi_lan_a, &socket_a,
                             discovered_service_info = discovered_service_info,
                             service_type, &server_socket, &flag]() {
      socket_a = wifi_lan_a.ConnectToService(discovered_service_info, &flag);
      if (!socket_a.IsValid()) {
        server_socket.Close();
      }
    });
    server_executor.Execute([&socket_b, &server_socket]() {
      socket_b = server_socket.Accept();
      if (!socket_b.IsValid()) {
        server_socket.Close();
      }
    });
  }
  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(socket_a.IsValid());
    EXPECT_TRUE(socket_b.IsValid());
  } else {
    EXPECT_FALSE(socket_a.IsValid());
    EXPECT_FALSE(socket_b.IsValid());
  }
  server_socket.Close();
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedWifiLanMediumTest, WifiLanMediumTest,
                         ::testing::ValuesIn(kTestCases));

TEST_F(WifiLanMediumTest, ConstructorDestructorWorks) {
  env_.Start();
  WifiLanMedium wifi_lan_a;
  WifiLanMedium wifi_lan_b;

  // Make sure we can create functional mediums.
  ASSERT_TRUE(wifi_lan_a.IsValid());
  ASSERT_TRUE(wifi_lan_b.IsValid());

  // Make sure we can create 2 distinct mediums.
  EXPECT_NE(&wifi_lan_a.GetImpl(), &wifi_lan_b.GetImpl());
  env_.Stop();
}

TEST_F(WifiLanMediumTest, CanStartAdvertising) {
  env_.Start();
  WifiLanMedium wifi_lan_a;
  std::string service_type(kServiceType);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);

  WifiLanServerSocket server_socket = wifi_lan_a.ListenForService();
  EXPECT_TRUE(server_socket.IsValid());

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  nsd_service_info.SetServiceType(service_type);
  EXPECT_TRUE(wifi_lan_a.StartAdvertising(nsd_service_info));
  EXPECT_TRUE(wifi_lan_a.StopAdvertising(nsd_service_info));
  env_.Stop();
}

TEST_F(WifiLanMediumTest, CanStartMultipleAdvertising) {
  env_.Start();
  WifiLanMedium wifi_lan_a;
  std::string service_type_1(kServiceType);
  std::string service_type_2("_service_1.tcp_");
  std::string service_info_name_1(kServiceInfoName);
  std::string service_info_name_2(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);

  WifiLanServerSocket server_socket = wifi_lan_a.ListenForService();
  EXPECT_TRUE(server_socket.IsValid());

  NsdServiceInfo nsd_service_info_1;
  nsd_service_info_1.SetServiceName(service_info_name_1);
  nsd_service_info_1.SetTxtRecord(std::string(kEndpointInfoKey),
                                  endpoint_info_name);
  nsd_service_info_1.SetServiceType(service_type_1);

  NsdServiceInfo nsd_service_info_2;
  nsd_service_info_2.SetServiceName(service_info_name_2);
  nsd_service_info_2.SetTxtRecord(std::string(kEndpointInfoKey),
                                  endpoint_info_name);
  nsd_service_info_2.SetServiceType(service_type_2);

  EXPECT_TRUE(wifi_lan_a.StartAdvertising(nsd_service_info_1));
  EXPECT_TRUE(wifi_lan_a.StartAdvertising(nsd_service_info_2));
  EXPECT_TRUE(wifi_lan_a.StopAdvertising(nsd_service_info_1));
  EXPECT_TRUE(wifi_lan_a.StopAdvertising(nsd_service_info_2));
  env_.Stop();
}

TEST_F(WifiLanMediumTest, CanStartDiscovery) {
  env_.Start();
  WifiLanMedium wifi_lan_a;
  std::string service_id(kServiceId);
  std::string service_type(kServiceType);

  EXPECT_TRUE(wifi_lan_a.StartDiscovery(service_id, service_type,
                                        DiscoveredServiceCallback{}));
  EXPECT_TRUE(wifi_lan_a.StopDiscovery(service_type));
  env_.Stop();
}

TEST_F(WifiLanMediumTest, CanStartMultipleDiscovery) {
  env_.Start();
  WifiLanMedium wifi_lan_a;
  std::string service_id_1(kServiceId);
  std::string service_id_2("service_id_2");
  std::string service_type_1(kServiceType);
  std::string service_type_2("_service_1.tcp_");

  EXPECT_TRUE(wifi_lan_a.StartDiscovery(service_id_1, service_type_1,
                                        DiscoveredServiceCallback{}));
  EXPECT_TRUE(wifi_lan_a.StartDiscovery(service_id_2, service_type_2,
                                        DiscoveredServiceCallback{}));
  EXPECT_TRUE(wifi_lan_a.StopDiscovery(service_type_1));
  EXPECT_TRUE(wifi_lan_a.StopDiscovery(service_type_2));
  env_.Stop();
}

TEST_F(WifiLanMediumTest, CanAdvertiseThatOtherMediumDiscover) {
  env_.Start();
  WifiLanMedium wifi_lan_a;
  WifiLanMedium wifi_lan_b;
  std::string service_id(kServiceId);
  std::string service_type(kServiceType);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  CountDownLatch discovered_latch(1);
  CountDownLatch lost_latch(1);

  wifi_lan_b.StartDiscovery(
      service_id, service_type,
      DiscoveredServiceCallback{
          .service_discovered_cb =
              [&discovered_latch](NsdServiceInfo service_info,
                                  const std::string& service_type) {
                discovered_latch.CountDown();
              },
          .service_lost_cb =
              [&lost_latch](NsdServiceInfo service_info,
                            const std::string& service_id) {
                lost_latch.CountDown();
              },
      });

  WifiLanServerSocket server_socket = wifi_lan_a.ListenForService();
  EXPECT_TRUE(server_socket.IsValid());

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  nsd_service_info.SetServiceType(service_type);
  EXPECT_TRUE(wifi_lan_a.StartAdvertising(nsd_service_info));
  EXPECT_TRUE(discovered_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(wifi_lan_a.StopAdvertising(nsd_service_info));
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(wifi_lan_b.StopDiscovery(service_type));
  env_.Stop();
}

TEST_F(WifiLanMediumTest, CanDiscoverMultipleAdvertisementsOnSameService) {
  env_.Start();
  WifiLanMedium wifi_lan_discovery;
  WifiLanMedium wifi_lan_advertising_1;
  WifiLanMedium wifi_lan_advertising_2;
  std::string service_id(kServiceId);
  std::string service_type(kServiceType);

  CountDownLatch discovered_latch(2);
  CountDownLatch lost_latch(2);

  wifi_lan_discovery.StartDiscovery(
      service_id, service_type,
      DiscoveredServiceCallback{
          .service_discovered_cb =
              [&discovered_latch](NsdServiceInfo service_info,
                                  const std::string& service_type) {
                discovered_latch.CountDown();
              },
          .service_lost_cb =
              [&lost_latch](NsdServiceInfo service_info,
                            const std::string& service_id) {
                lost_latch.CountDown();
              },
      });

  // Setup first advertising device.
  WifiLanServerSocket server_socket_1 =
      wifi_lan_advertising_1.ListenForService();
  EXPECT_TRUE(server_socket_1.IsValid());

  NsdServiceInfo nsd_service_info_1;
  nsd_service_info_1.SetServiceName("service1");
  nsd_service_info_1.SetTxtRecord(std::string(kEndpointInfoKey), "endpoint1");
  nsd_service_info_1.SetServiceType(service_type);

  // Setup second advertising device.
  WifiLanServerSocket server_socket_2 =
      wifi_lan_advertising_2.ListenForService();
  EXPECT_TRUE(server_socket_2.IsValid());

  NsdServiceInfo nsd_service_info_2;
  nsd_service_info_2.SetServiceName("service2");
  nsd_service_info_2.SetTxtRecord(std::string(kEndpointInfoKey), "endpoint2");
  nsd_service_info_2.SetServiceType(service_type);

  EXPECT_TRUE(wifi_lan_advertising_1.StartAdvertising(nsd_service_info_1));
  EXPECT_TRUE(wifi_lan_advertising_2.StartAdvertising(nsd_service_info_2));
  EXPECT_TRUE(discovered_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(wifi_lan_advertising_1.StopAdvertising(nsd_service_info_1));
  EXPECT_TRUE(wifi_lan_advertising_2.StopAdvertising(nsd_service_info_2));
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());

  // Stop to descovery
  EXPECT_TRUE(wifi_lan_discovery.StopDiscovery(service_type));
  env_.Stop();
}

TEST_F(WifiLanMediumTest, CanDiscoverThatOtherMediumAdvertise) {
  env_.Start();
  WifiLanMedium wifi_lan_a;
  WifiLanMedium wifi_lan_b;
  std::string service_id(kServiceId);
  std::string service_type(kServiceType);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  CountDownLatch discovered_latch(1);
  CountDownLatch lost_latch(1);

  wifi_lan_a.StartDiscovery(
      service_id, service_type,
      DiscoveredServiceCallback{
          .service_discovered_cb =
              [&discovered_latch](NsdServiceInfo service_info,
                                  const std::string& service_type) {
                discovered_latch.CountDown();
              },
          .service_lost_cb =
              [&lost_latch](NsdServiceInfo service_info,
                            const std::string& service_type) {
                lost_latch.CountDown();
              },
      });

  WifiLanServerSocket server_socket = wifi_lan_a.ListenForService();
  EXPECT_TRUE(server_socket.IsValid());

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  nsd_service_info.SetServiceType(service_type);
  EXPECT_TRUE(wifi_lan_b.StartAdvertising(nsd_service_info));
  EXPECT_TRUE(discovered_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(wifi_lan_b.StopAdvertising(nsd_service_info));
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(wifi_lan_a.StopDiscovery(service_type));
  env_.Stop();
}

}  // namespace
}  // namespace nearby
