
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

#include "connections/implementation/mediums/wifi_hotspot.h"

#include <cstddef>
#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/wifi_hotspot.h"
#include "internal/platform/wifi_credential.h"

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

constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kSsid{"Direct-357a2d8c"};
constexpr absl::string_view kPassword{"12345678"};
constexpr absl::string_view kIp = "123.234.23.1";
constexpr int kFrequency = 2412;
constexpr const size_t kPort = 20;

class WifiHotspotTest : public testing::TestWithParam<FeatureFlags> {
 protected:
  WifiHotspotTest() {
    env_.Stop();
    env_.Start();
  }
  ~WifiHotspotTest() override{
    env_.Stop();
  }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

INSTANTIATE_TEST_SUITE_P(ParametrisedWifiHotspotMediumTest, WifiHotspotTest,
                         testing::ValuesIn(kTestCases));

TEST_F(WifiHotspotTest, ConstructorDestructorWorks) {
  auto wifi_hotspot_a = std::make_unique<WifiHotspot>();
  auto wifi_hotspot_b = std::make_unique<WifiHotspot>();

  EXPECT_TRUE(wifi_hotspot_a->IsClientAvailable());
  EXPECT_TRUE(wifi_hotspot_b->IsClientAvailable());
}

TEST_F(WifiHotspotTest, CanStartStopHotspot) {
  std::string service_id(kServiceID);
  auto wifi_hotspot_a = std::make_unique<WifiHotspot>();

  if (wifi_hotspot_a->IsAPAvailable()) {
    EXPECT_TRUE(wifi_hotspot_a->StartWifiHotspot());
    EXPECT_TRUE(wifi_hotspot_a->StartAcceptingConnections(service_id, {}));
    EXPECT_TRUE(wifi_hotspot_a->StopWifiHotspot());
  } else {
    EXPECT_FALSE(wifi_hotspot_a->StartWifiHotspot());
  }
}

TEST_F(WifiHotspotTest, CanConnectDisconnectHotspot) {
  auto wifi_hotspot_a = std::make_unique<WifiHotspot>();
  std::string ssid(kSsid);
  std::string password(kPassword);

  EXPECT_FALSE(wifi_hotspot_a->ConnectWifiHotspot(ssid, password, kFrequency));
  EXPECT_TRUE(wifi_hotspot_a->DisconnectWifiHotspot());
}

TEST_P(WifiHotspotTest, CanStartHotspotThatOtherConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);

  std::string service_id(kServiceID);
  std::string ip(kIp);
  auto wifi_hotspot_a = std::make_unique<WifiHotspot>();
  auto wifi_hotspot_b = std::make_unique<WifiHotspot>();

  EXPECT_TRUE(wifi_hotspot_a->StartWifiHotspot());
  if (!wifi_hotspot_a->IsAcceptingConnections(service_id)) {
    EXPECT_TRUE(wifi_hotspot_a->StartAcceptingConnections(service_id, {}));
  }

  HotspotCredentials* hotspot_credentials =
      wifi_hotspot_a->GetCredentials(service_id);

  EXPECT_TRUE(wifi_hotspot_b->ConnectWifiHotspot(
      hotspot_credentials->GetSSID(), hotspot_credentials->GetPassword(),
      hotspot_credentials->GetFrequency()));

  WifiHotspotSocket socket_client;
  EXPECT_FALSE(socket_client.IsValid());

  CancellationFlag flag;
  socket_client = wifi_hotspot_b->Connect(service_id, ip, kPort, &flag);
  EXPECT_FALSE(socket_client.IsValid());

  socket_client =
      wifi_hotspot_b->Connect(service_id, hotspot_credentials->GetGateway(),
                              hotspot_credentials->GetPort(), &flag);
  EXPECT_TRUE(socket_client.IsValid());

  EXPECT_TRUE(wifi_hotspot_b->DisconnectWifiHotspot());
  EXPECT_TRUE(wifi_hotspot_a->StopWifiHotspot());
}

TEST_P(WifiHotspotTest, CanStartHotspotThatOtherCanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);

  std::string service_id(kServiceID);
  std::string ip(kIp);
  auto wifi_hotspot_a = std::make_unique<WifiHotspot>();
  auto wifi_hotspot_b = std::make_unique<WifiHotspot>();

  EXPECT_TRUE(wifi_hotspot_a->StartWifiHotspot());
  if (!wifi_hotspot_a->IsAcceptingConnections(service_id)) {
    EXPECT_TRUE(wifi_hotspot_a->StartAcceptingConnections(service_id, {}));
  }

  HotspotCredentials* hotspot_credentials =
      wifi_hotspot_a->GetCredentials(service_id);

  EXPECT_TRUE(wifi_hotspot_b->ConnectWifiHotspot(
      hotspot_credentials->GetSSID(), hotspot_credentials->GetPassword(),
      hotspot_credentials->GetFrequency()));

  WifiHotspotSocket socket_client;
  EXPECT_FALSE(socket_client.IsValid());

  CancellationFlag flag(true);
  socket_client =
      wifi_hotspot_b->Connect(service_id, hotspot_credentials->GetGateway(),
                              hotspot_credentials->GetPort(), &flag);

  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(socket_client.IsValid());
    EXPECT_TRUE(wifi_hotspot_b->DisconnectWifiHotspot());
    EXPECT_TRUE(wifi_hotspot_a->StopWifiHotspot());
  } else {
    EXPECT_FALSE(socket_client.IsValid());
    EXPECT_TRUE(wifi_hotspot_b->DisconnectWifiHotspot());
    EXPECT_TRUE(wifi_hotspot_a->StopWifiHotspot());
  }
}

TEST_F(WifiHotspotTest, CanStartHotspotTheOtherFailConnect) {
  auto wifi_hotspot_a = std::make_unique<WifiHotspot>();
  auto wifi_hotspot_b = std::make_unique<WifiHotspot>();

  EXPECT_TRUE(wifi_hotspot_a->StartWifiHotspot());

  std::string ssid(kSsid);
  std::string password(kPassword);

  EXPECT_FALSE(wifi_hotspot_b->ConnectWifiHotspot(ssid, password, kFrequency));
  EXPECT_TRUE(wifi_hotspot_b->DisconnectWifiHotspot());

  EXPECT_TRUE(wifi_hotspot_a->StopWifiHotspot());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
