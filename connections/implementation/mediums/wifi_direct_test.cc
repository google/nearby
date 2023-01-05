
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

#include "connections/implementation/mediums/wifi_direct.h"

#include <string>

#include "gtest/gtest.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/wifi_direct.h"

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
constexpr const size_t kPort = 20;

class WifiDirectTest : public testing::TestWithParam<FeatureFlags> {
 protected:
  WifiDirectTest() {
    env_.Stop();
    env_.Start();
  }
  ~WifiDirectTest() override { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

INSTANTIATE_TEST_SUITE_P(ParametrisedWifiDirectMediumTest, WifiDirectTest,
                         testing::ValuesIn(kTestCases));

TEST_F(WifiDirectTest, ConstructorDestructorWorks) {
  WifiDirect wifi_direct_a, wifi_direct_b;

  EXPECT_NE(&wifi_direct_a, &wifi_direct_b);
  EXPECT_TRUE(wifi_direct_a.IsGCAvailable());
  EXPECT_TRUE(wifi_direct_b.IsGCAvailable());
}
TEST_F(WifiDirectTest, CanStartStopGO) {
  std::string service_id(kServiceID);
  WifiDirect wifi_direct_a;

  if (wifi_direct_a.IsGOAvailable()) {
    EXPECT_FALSE(wifi_direct_a.IsGOStarted());
    EXPECT_TRUE(wifi_direct_a.StartWifiDirect());
    EXPECT_TRUE(wifi_direct_a.StartAcceptingConnections(service_id, {}));
    EXPECT_TRUE(wifi_direct_a.IsGOStarted());
    EXPECT_TRUE(wifi_direct_a.StopWifiDirect());
    EXPECT_FALSE(wifi_direct_a.IsGOStarted());
  } else {
    EXPECT_FALSE(wifi_direct_a.StartWifiDirect());
  }
}

TEST_F(WifiDirectTest, GCCanConnectDisconnectGO) {
  std::string ssid(kSsid);
  std::string password(kPassword);
  WifiDirect wifi_direct_a;

  EXPECT_FALSE(wifi_direct_a.ConnectWifiDirect(ssid, password));
  EXPECT_TRUE(wifi_direct_a.DisconnectWifiDirect());
}

TEST_P(WifiDirectTest, CanStartGOThatOtherConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);

  std::string service_id(kServiceID);
  std::string ip(kIp);
  WifiDirect wifi_direct_a, wifi_direct_b;

  EXPECT_FALSE(wifi_direct_b.IsConnectedToGO());
  EXPECT_TRUE(wifi_direct_a.StartWifiDirect());
  if (!wifi_direct_a.IsAcceptingConnections(service_id)) {
    EXPECT_TRUE(wifi_direct_a.StartAcceptingConnections(service_id, {}));
  }

  WifiDirectCredentials* wifi_direct_credentials =
      wifi_direct_a.GetCredentials(service_id);

  EXPECT_TRUE(
      wifi_direct_b.ConnectWifiDirect(wifi_direct_credentials->GetSSID(),
                                       wifi_direct_credentials->GetPassword()));
  EXPECT_TRUE(wifi_direct_b.IsConnectedToGO());

  WifiDirectSocket socket_client;
  EXPECT_FALSE(socket_client.IsValid());

  CancellationFlag flag;
  socket_client = wifi_direct_b.Connect(service_id, ip, kPort, &flag);
  EXPECT_FALSE(socket_client.IsValid());

  socket_client =
      wifi_direct_b.Connect(service_id, wifi_direct_credentials->GetGateway(),
                             wifi_direct_credentials->GetPort(), &flag);
  EXPECT_TRUE(socket_client.IsValid());

  EXPECT_TRUE(wifi_direct_b.DisconnectWifiDirect());
  EXPECT_FALSE(wifi_direct_b.IsConnectedToGO());
  EXPECT_TRUE(wifi_direct_a.StopWifiDirect());
}

TEST_P(WifiDirectTest, CanStartGOThatOtherCanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);

  std::string service_id(kServiceID);
  std::string ip(kIp);
  WifiDirect wifi_direct_a, wifi_direct_b;

  EXPECT_TRUE(wifi_direct_a.StartWifiDirect());
  if (!wifi_direct_a.IsAcceptingConnections(service_id)) {
    EXPECT_TRUE(wifi_direct_a.StartAcceptingConnections(service_id, {}));
  }

  WifiDirectCredentials* wifi_direct_credentials =
      wifi_direct_a.GetCredentials(service_id);

  EXPECT_TRUE(
      wifi_direct_b.ConnectWifiDirect(wifi_direct_credentials->GetSSID(),
                                       wifi_direct_credentials->GetPassword()));

  WifiDirectSocket socket_client;
  EXPECT_FALSE(socket_client.IsValid());

  CancellationFlag flag(true);
  socket_client =
      wifi_direct_b.Connect(service_id, wifi_direct_credentials->GetGateway(),
                             wifi_direct_credentials->GetPort(), &flag);

  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(socket_client.IsValid());
    EXPECT_TRUE(wifi_direct_b.DisconnectWifiDirect());
    EXPECT_TRUE(wifi_direct_a.StopWifiDirect());
  } else {
    EXPECT_FALSE(socket_client.IsValid());
    EXPECT_TRUE(wifi_direct_b.DisconnectWifiDirect());
    EXPECT_TRUE(wifi_direct_a.StopWifiDirect());
  }
}

TEST_F(WifiDirectTest, CanStartGOTheOtherFailConnect) {
  WifiDirect wifi_direct_a, wifi_direct_b;

  EXPECT_TRUE(wifi_direct_a.StartWifiDirect());

  std::string ssid(kSsid);
  std::string password(kPassword);

  EXPECT_FALSE(wifi_direct_b.ConnectWifiDirect(ssid, password));
  EXPECT_TRUE(wifi_direct_b.DisconnectWifiDirect());
  EXPECT_TRUE(wifi_direct_a.StopWifiDirect());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
