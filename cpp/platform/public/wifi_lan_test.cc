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

#include "platform/public/wifi_lan.h"

#include <memory>

#include "platform/base/medium_environment.h"
#include "platform/public/count_down_latch.h"
#include "platform/public/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"

namespace location {
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

constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kServiceInfoName{"Simulated service info name"};
constexpr absl::string_view kEndpointName{"Simulated endpoint name"};
constexpr absl::string_view kEndpointInfoKey{"n"};

class WifiLanMediumTest : public ::testing::TestWithParam<FeatureFlags> {
 protected:
  using DiscoveredServiceCallback = WifiLanMedium::DiscoveredServiceCallback;
  using AcceptedConnectionCallback = WifiLanMedium::AcceptedConnectionCallback;

  WifiLanMediumTest() { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(WifiLanMediumTest, CanStartAcceptingConnectionsAndConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  WifiLanMedium wifi_a;
  WifiLanMedium wifi_b;
  std::string service_id(kServiceID);
  std::string service_info_name{kServiceInfoName};
  std::string endpoint_info_name{kEndpointName};
  CountDownLatch found_latch(1);
  CountDownLatch accepted_latch(1);

  WifiLanService* discovered_service = nullptr;
  wifi_a.StartDiscovery(
      service_id,
      DiscoveredServiceCallback{
          .service_discovered_cb =
              [&found_latch, &discovered_service](
                  WifiLanService& service, const std::string& service_id) {
                NEARBY_LOG(
                    INFO, "Service discovered: %s, %p",
                    service.GetServiceInfo().GetServiceInfoName().c_str(),
                    &service);
                discovered_service = &service;
                found_latch.CountDown();
              },
      });

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceInfoName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  wifi_b.StartAdvertising(service_id, nsd_service_info);
  wifi_b.StartAcceptingConnections(
      service_id,
      AcceptedConnectionCallback{
          .accepted_cb = [&accepted_latch](WifiLanSocket socket,
                                           const std::string& service_id) {
            NEARBY_LOG(INFO, "Connection accepted: socket=%p, service_id=%s",
                       &socket, service_id.c_str());
            accepted_latch.CountDown();
          }});
  EXPECT_TRUE(found_latch.Await(absl::Milliseconds(1000)).result());

  WifiLanSocket socket_a;
  EXPECT_FALSE(socket_a.IsValid());
  {
    SingleThreadExecutor client_executor;
    client_executor.Execute(
        [&wifi_a, &socket_a, discovered_service, &service_id]() {
          CancellationFlag flag;
          socket_a = wifi_a.Connect(*discovered_service, service_id, &flag);
        });
  }
  EXPECT_TRUE(accepted_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_TRUE(socket_a.IsValid());
  wifi_b.StopAcceptingConnections(service_id);
  wifi_b.StopAdvertising(service_id);
  wifi_a.StopDiscovery(service_id);
  env_.Stop();
}

TEST_P(WifiLanMediumTest, CanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  WifiLanMedium wifi_a;
  WifiLanMedium wifi_b;
  std::string service_id(kServiceID);
  std::string service_info_name{kServiceInfoName};
  std::string endpoint_info_name{kEndpointName};
  CountDownLatch found_latch(1);
  CountDownLatch accepted_latch(1);

  WifiLanService* discovered_service = nullptr;
  wifi_a.StartDiscovery(
      service_id,
      DiscoveredServiceCallback{
          .service_discovered_cb =
              [&found_latch, &discovered_service](
                  WifiLanService& service, const std::string& service_id) {
                NEARBY_LOG(
                    INFO, "Service discovered: %s, %p",
                    service.GetServiceInfo().GetServiceInfoName().c_str(),
                    &service);
                discovered_service = &service;
                found_latch.CountDown();
              },
      });

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceInfoName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  wifi_b.StartAdvertising(service_id, nsd_service_info);
  wifi_b.StartAcceptingConnections(
      service_id,
      AcceptedConnectionCallback{
          .accepted_cb = [&accepted_latch](WifiLanSocket socket,
                                           const std::string& service_id) {
            NEARBY_LOG(INFO, "Connection accepted: socket=%p, service_id=%s",
                       &socket, service_id.c_str());
            accepted_latch.CountDown();
          }});
  EXPECT_TRUE(found_latch.Await(absl::Milliseconds(1000)).result());

  WifiLanSocket socket_a;
  EXPECT_FALSE(socket_a.IsValid());
  {
    SingleThreadExecutor client_executor;
    client_executor.Execute(
        [&wifi_a, &socket_a, discovered_service, &service_id]() {
          // Make it as Cancelled.
          CancellationFlag flag(true);
          socket_a = wifi_a.Connect(*discovered_service, service_id, &flag);
        });
  }

  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(accepted_latch.Await(absl::Milliseconds(1000)).result());
    EXPECT_TRUE(socket_a.IsValid());
  } else {
    EXPECT_FALSE(accepted_latch.Await(absl::Milliseconds(1000)).result());
    EXPECT_FALSE(socket_a.IsValid());
  }

  wifi_b.StopAcceptingConnections(service_id);
  wifi_b.StopAdvertising(service_id);
  wifi_a.StopDiscovery(service_id);
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedWifiLanMediumTest, WifiLanMediumTest,
                         ::testing::ValuesIn(kTestCases));

TEST_F(WifiLanMediumTest, ConstructorDestructorWorks) {
  env_.Start();
  WifiLanMedium wifi_a;
  WifiLanMedium wifi_b;

  // Make sure we can create functional mediums.
  ASSERT_TRUE(wifi_a.IsValid());
  ASSERT_TRUE(wifi_b.IsValid());

  // Make sure we can create 2 distinct mediums.
  EXPECT_NE(&wifi_a.GetImpl(), &wifi_b.GetImpl());
  env_.Stop();
}

TEST_F(WifiLanMediumTest, CanStartAdvertising) {
  env_.Start();
  WifiLanMedium wifi_a;
  WifiLanMedium wifi_b;
  std::string service_id(kServiceID);
  std::string service_info_name{kServiceInfoName};
  std::string endpoint_info_name{kEndpointName};
  CountDownLatch found_latch(1);

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceInfoName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  wifi_a.StartAdvertising(service_id, nsd_service_info);

  EXPECT_TRUE(wifi_b.StartDiscovery(
      service_id, DiscoveredServiceCallback{
                      .service_discovered_cb =
                          [&found_latch](WifiLanService& service,
                                         const std::string& service_id) {
                            found_latch.CountDown();
                          },
                  }));
  EXPECT_TRUE(found_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_TRUE(wifi_a.StopAdvertising(service_id));
  EXPECT_TRUE(wifi_b.StopDiscovery(service_id));
  env_.Stop();
}

TEST_F(WifiLanMediumTest, CanStartDiscovery) {
  env_.Start();
  WifiLanMedium wifi_a;
  WifiLanMedium wifi_b;
  std::string service_id(kServiceID);
  std::string service_info_name{kServiceInfoName};
  std::string endpoint_info_name{kEndpointName};
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  wifi_a.StartDiscovery(service_id,
                        DiscoveredServiceCallback{
                            .service_discovered_cb =
                                [&found_latch](WifiLanService& service,
                                               absl::string_view service_id) {
                                  found_latch.CountDown();
                                },
                            .service_lost_cb =
                                [&lost_latch](WifiLanService& service,
                                              absl::string_view service_id) {
                                  lost_latch.CountDown();
                                },
                        });

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceInfoName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  EXPECT_TRUE(wifi_b.StartAdvertising(service_id, nsd_service_info));
  EXPECT_TRUE(found_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_TRUE(wifi_b.StopAdvertising(service_id));
  EXPECT_TRUE(lost_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_TRUE(wifi_a.StopDiscovery(service_id));
  env_.Stop();
}

TEST_F(WifiLanMediumTest, CanStopDiscovery) {
  env_.Start();
  WifiLanMedium wifi_a;
  WifiLanMedium wifi_b;
  std::string service_id(kServiceID);
  std::string service_info_name{kServiceInfoName};
  std::string endpoint_info_name{kEndpointName};
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  wifi_a.StartDiscovery(service_id,
                        DiscoveredServiceCallback{
                            .service_discovered_cb =
                                [&found_latch](WifiLanService& service,
                                               absl::string_view service_id) {
                                  found_latch.CountDown();
                                },
                            .service_lost_cb =
                                [&lost_latch](WifiLanService& service,
                                              absl::string_view service_id) {
                                  lost_latch.CountDown();
                                },
                        });

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceInfoName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);

  EXPECT_TRUE(wifi_b.StartAdvertising(service_id, nsd_service_info));
  EXPECT_TRUE(found_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_TRUE(wifi_a.StopDiscovery(service_id));
  EXPECT_TRUE(wifi_b.StopAdvertising(service_id));
  EXPECT_FALSE(lost_latch.Await(absl::Milliseconds(1000)).result());
  env_.Stop();
}

}  // namespace
}  // namespace nearby
}  // namespace location
