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

#include "platform_v2/public/wifi_lan.h"

#include <memory>

#include "platform_v2/base/medium_environment.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {
namespace {

constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kServiceInfoName{"Simulated service info name"};

class WifiLanMediumTest : public ::testing::Test {
 protected:
  using DiscoveredServiceCallback = WifiLanMedium::DiscoveredServiceCallback;
  using AcceptedConnectionCallback = WifiLanMedium::AcceptedConnectionCallback;

  WifiLanMediumTest() { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

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
  CountDownLatch found_latch(1);

  wifi_a.StartAdvertising(service_id, service_info_name);

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
  EXPECT_TRUE(wifi_b.StartAdvertising(service_id, service_info_name));
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
  EXPECT_TRUE(wifi_b.StartAdvertising(service_id, service_info_name));
  EXPECT_TRUE(found_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_TRUE(wifi_a.StopDiscovery(service_id));
  EXPECT_TRUE(wifi_b.StopAdvertising(service_id));
  EXPECT_FALSE(lost_latch.Await(absl::Milliseconds(1000)).result());
  env_.Stop();
}

TEST_F(WifiLanMediumTest, CanStartAcceptingConnectionsAndConnect) {
  env_.Start();
  WifiLanMedium wifi_a;
  WifiLanMedium wifi_b;
  std::string service_id(kServiceID);
  std::string service_info_name{kServiceInfoName};
  CountDownLatch found_latch(1);
  CountDownLatch accepted_latch(1);

  WifiLanService* discovered_service = nullptr;
  wifi_a.StartDiscovery(
      service_id,
      DiscoveredServiceCallback{
          .service_discovered_cb =
              [&found_latch, &discovered_service](
                  WifiLanService& service, const std::string& service_id) {
                NEARBY_LOG(INFO, "Service discovered: %s, %p",
                           service.GetName().c_str(), &service);
                discovered_service = &service;
                found_latch.CountDown();
              },
      });
  wifi_b.StartAdvertising(service_id, service_info_name);
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
          socket_a = wifi_a.Connect(*discovered_service, service_id);
        });
  }
  EXPECT_TRUE(accepted_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_TRUE(socket_a.IsValid());
  wifi_b.StopAcceptingConnections(service_id);
  wifi_b.StopAdvertising(service_id);
  wifi_a.StopDiscovery(service_id);
  env_.Stop();
}

}  // namespace
}  // namespace nearby
}  // namespace location
