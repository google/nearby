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

#include "core/internal/mediums/wifi_lan.h"

#include <string>

#include "platform/base/medium_environment.h"
#include "platform/public/count_down_latch.h"
#include "platform/public/logging.h"
#include "platform/public/wifi_lan.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kServiceInfoName{
    "Simulated WifiLan service encrypted string #1"};
constexpr absl::string_view kEndpointName{"Simulated endpoint name"};
constexpr absl::string_view kEndpointInfoKey{"n"};

class WifiLanTest : public ::testing::Test {
 protected:
  using DiscoveredServiceCallback = WifiLanMedium::DiscoveredServiceCallback;

  WifiLanTest() { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

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
  WifiLan wifi_lan_b;
  std::string service_id(kServiceID);
  std::string service_info_name{kServiceInfoName};
  std::string endpoint_info_name{kEndpointName};
  CountDownLatch found_latch(1);

  wifi_lan_b.StartDiscovery(
      service_id, DiscoveredServiceCallback{
                      .service_discovered_cb =
                          [&found_latch](WifiLanService& service,
                                         absl::string_view service_id) {
                            found_latch.CountDown();
                          },
                  });

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceInfoName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  EXPECT_TRUE(wifi_lan_a.StartAdvertising(service_id, nsd_service_info));
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(wifi_lan_a.StopAdvertising(service_id));
  EXPECT_TRUE(wifi_lan_b.StopDiscovery(service_id));
  env_.Stop();
}

TEST_F(WifiLanTest, CanStartDiscovery) {
  env_.Start();
  WifiLan wifi_lan_a;
  WifiLan wifi_lan_b;
  std::string service_id(kServiceID);
  std::string service_info_name{kServiceInfoName};
  std::string endpoint_info_name{kEndpointName};
  CountDownLatch accept_latch(1);
  CountDownLatch lost_latch(1);

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceInfoName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  wifi_lan_b.StartAdvertising(service_id, nsd_service_info);

  EXPECT_TRUE(wifi_lan_a.StartDiscovery(
      service_id, {
                      .service_discovered_cb =
                          [&accept_latch](WifiLanService& service,
                                          const std::string& service_id) {
                            accept_latch.CountDown();
                          },
                      .service_lost_cb =
                          [&lost_latch](WifiLanService& service,
                                        const std::string& service_id) {
                            lost_latch.CountDown();
                          },
                  }));
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  wifi_lan_b.StopAdvertising(service_id);
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(wifi_lan_a.StopDiscovery(service_id));
  env_.Stop();
}

TEST_F(WifiLanTest, CanStartAcceptingConnectionsAndConnect) {
  env_.Start();
  WifiLan wifi_lan_a;
  WifiLan wifi_lan_b;
  std::string service_id(kServiceID);
  std::string service_info_name{kServiceInfoName};
  std::string endpoint_info_name{kEndpointName};
  CountDownLatch found_latch(1);
  CountDownLatch accept_latch(1);

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceInfoName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  wifi_lan_a.StartAdvertising(service_id, nsd_service_info);
  wifi_lan_a.StartAcceptingConnections(
      service_id,
      {
          .accepted_cb = [&accept_latch](
                             WifiLanSocket socket,
                             absl::string_view) { accept_latch.CountDown(); },
      });
  WifiLanService discovered_service;
  wifi_lan_b.StartDiscovery(
      service_id,
      {
          .service_discovered_cb =
              [&found_latch, &discovered_service](
                  WifiLanService& service, absl::string_view service_id) {
                discovered_service = service;
                NEARBY_LOG(INFO, "Discovered service=%p [impl=%p]", &service,
                           &service.GetImpl());
                found_latch.CountDown();
              },
      });

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  ASSERT_TRUE(discovered_service.IsValid());

  WifiLanSocket socket =
      wifi_lan_b.Connect(discovered_service, service_id);

  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(socket.IsValid());
  wifi_lan_b.StopDiscovery(service_id);
  wifi_lan_a.StopAcceptingConnections(service_id);
  wifi_lan_a.StopAdvertising(service_id);
  env_.Stop();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
