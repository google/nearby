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

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "platform/base/medium_environment.h"
#include "platform/public/count_down_latch.h"
#include "platform/public/logging.h"
#include "platform/public/wifi_lan_v2.h"

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

constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kServiceId{"service_id"};
constexpr absl::string_view kServiceType{"_service.tcp_"};
constexpr absl::string_view kServiceInfoName{"Simulated service info name"};
constexpr absl::string_view kEndpointName{"Simulated endpoint name"};
constexpr absl::string_view kEndpointInfoKey{"n"};

class WifiLanMediumV2Test : public ::testing::TestWithParam<FeatureFlags> {
 protected:
  using DiscoveredServiceCallback = WifiLanMediumV2::DiscoveredServiceCallback;

  WifiLanMediumV2Test() { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(WifiLanMediumV2Test, ConstructorDestructorWorks) {
  env_.Start();
  WifiLanMediumV2 wifi_lan_a;
  WifiLanMediumV2 wifi_lan_b;

  // Make sure we can create functional mediums.
  ASSERT_TRUE(wifi_lan_a.IsValid());
  ASSERT_TRUE(wifi_lan_b.IsValid());

  // Make sure we can create 2 distinct mediums.
  EXPECT_NE(&wifi_lan_a.GetImpl(), &wifi_lan_b.GetImpl());
  env_.Stop();
}

TEST_F(WifiLanMediumV2Test, CanStartAdvertising) {
  env_.Start();
  WifiLanMediumV2 wifi_lan_a;
  std::string service_type(kServiceType);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  nsd_service_info.SetServiceType(service_type);
  EXPECT_TRUE(wifi_lan_a.StartAdvertising(nsd_service_info));
  EXPECT_TRUE(wifi_lan_a.StopAdvertising(nsd_service_info));
  env_.Stop();
}

TEST_F(WifiLanMediumV2Test, CanStartMultipleAdvertising) {
  env_.Start();
  WifiLanMediumV2 wifi_lan_a;
  std::string service_type_1(kServiceType);
  std::string service_type_2("_service_1.tcp_");
  std::string service_info_name_1(kServiceInfoName);
  std::string service_info_name_2(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);

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

TEST_F(WifiLanMediumV2Test, CanStartDiscovery) {
  env_.Start();
  WifiLanMediumV2 wifi_lan_a;
  std::string service_id(kServiceId);
  std::string service_type(kServiceType);

  EXPECT_TRUE(wifi_lan_a.StartDiscovery(service_id, service_type,
                                        DiscoveredServiceCallback{}));
  EXPECT_TRUE(wifi_lan_a.StopDiscovery(service_type));
  env_.Stop();
}

TEST_F(WifiLanMediumV2Test, CanStartMultipleDiscovery) {
  env_.Start();
  WifiLanMediumV2 wifi_lan_a;
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

TEST_F(WifiLanMediumV2Test, CanAdvertiseThatOtherMediumDiscover) {
  env_.Start();
  WifiLanMediumV2 wifi_lan_a;
  WifiLanMediumV2 wifi_lan_b;
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

TEST_F(WifiLanMediumV2Test, CanDiscoverThatOtherMediumAdvertise) {
  env_.Start();
  WifiLanMediumV2 wifi_lan_a;
  WifiLanMediumV2 wifi_lan_b;
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
}  // namespace location
