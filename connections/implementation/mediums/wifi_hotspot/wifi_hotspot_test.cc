
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

#include "connections/implementation/mediums/wifi_hotspot/wifi_hotspot.h"

#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/cleanup/cleanup.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/expected.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/service_address.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/wifi_credential.h"
#include "internal/platform/wifi_hotspot.h"

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

class WifiHotspotTest : public testing::TestWithParam<FeatureFlags> {
 protected:
  WifiHotspotTest() {
    env_.Stop();
    env_.Start();
  }
  ~WifiHotspotTest() override { env_.Stop(); }

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

// Verifies that StartAcceptingConnections rejects an empty service_id.
TEST_F(WifiHotspotTest, StartAcceptingConnectionsFailsWithEmptyServiceId) {
  auto wifi_hotspot = std::make_unique<WifiHotspot>();

  EXPECT_FALSE(wifi_hotspot->StartAcceptingConnections(/*service_id=*/"",
                                                       /*callback=*/{}));
  EXPECT_FALSE(wifi_hotspot->IsAcceptingConnections(/*service_id=*/""));
}

// Verifies that a WifiHotspot instance in the Client (STA) role can join a
// remote SoftAP and establish an EndpointChannel to its listening socket.
TEST_F(WifiHotspotTest, ClientStaCanConnectToRemoteHotspot) {
  // Set up a remote SoftAP at the platform layer to simulate a peer (e.g.
  // Android) hosting a Wi-Fi Hotspot independent of the local WifiHotspot
  // medium's AP support.
  WifiHotspotMedium remote_ap_medium;
  ASSERT_TRUE(remote_ap_medium.StartWifiHotspot());
  WifiHotspotServerSocket server_socket = remote_ap_medium.ListenForService();
  ASSERT_TRUE(server_socket.IsValid());

  HotspotCredentials* remote_credentials = remote_ap_medium.GetCredential();
  ASSERT_NE(remote_credentials, nullptr);
  server_socket.PopulateHotspotCredentials(*remote_credentials);

  CountDownLatch accept_latch(1);
  WifiHotspotSocket accepted_socket;
  SingleThreadExecutor server_executor;
  absl::Cleanup cleanup = [&]() {
    server_socket.Close();
    if (accepted_socket.IsValid()) {
      accepted_socket.Close();
    }
    remote_ap_medium.StopWifiHotspot();
  };
  server_executor.Execute([&]() {
    accepted_socket = server_socket.Accept();
    if (accepted_socket.IsValid()) {
      accept_latch.CountDown();
    }
  });

  auto wifi_hotspot_client = std::make_unique<WifiHotspot>();
  EXPECT_TRUE(wifi_hotspot_client->IsClientAvailable());
  EXPECT_TRUE(wifi_hotspot_client->ConnectWifiHotspot(*remote_credentials));
  EXPECT_TRUE(wifi_hotspot_client->IsConnectedToHotspot());

  CancellationFlag flag;
  ErrorOr<std::unique_ptr<EndpointChannel>> channel_result =
      wifi_hotspot_client->Connect(std::string(kServiceID),
                                   remote_credentials->GetAddressCandidates(),
                                   &flag);
  ASSERT_TRUE(channel_result.has_value());
  std::unique_ptr<EndpointChannel> channel = std::move(channel_result.value());
  ASSERT_NE(channel, nullptr);
  EXPECT_TRUE(accept_latch.Await(absl::Milliseconds(1000)).result());

  channel->Close();
  EXPECT_TRUE(wifi_hotspot_client->DisconnectWifiHotspot());
  EXPECT_FALSE(wifi_hotspot_client->IsConnectedToHotspot());
}

TEST_F(WifiHotspotTest, CanConnectDisconnectHotspot) {
  auto wifi_hotspot_a = std::make_unique<WifiHotspot>();
  HotspotCredentials hotspot_credentials;
  hotspot_credentials.SetSSID(std::string(kSsid));
  hotspot_credentials.SetPassword(std::string(kPassword));

  EXPECT_FALSE(wifi_hotspot_a->ConnectWifiHotspot(hotspot_credentials));
  EXPECT_TRUE(wifi_hotspot_a->DisconnectWifiHotspot());
}

TEST_P(WifiHotspotTest, CanStartHotspotThatOtherConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);

  std::string service_id(kServiceID);
  auto wifi_hotspot_a = std::make_unique<WifiHotspot>();
  auto wifi_hotspot_b = std::make_unique<WifiHotspot>();

  EXPECT_TRUE(wifi_hotspot_a->StartWifiHotspot());
  if (!wifi_hotspot_a->IsAcceptingConnections(service_id)) {
    EXPECT_TRUE(wifi_hotspot_a->StartAcceptingConnections(service_id, {}));
  }

  HotspotCredentials* hotspot_credentials =
      wifi_hotspot_a->GetCredentials(service_id);

  EXPECT_TRUE(wifi_hotspot_b->ConnectWifiHotspot(*hotspot_credentials));

  ServiceAddress service_address = {
      .address = {123, 234, 23, 1},
      .port = 20,
  };
  CancellationFlag flag;
  ErrorOr<std::unique_ptr<EndpointChannel>> channel_result =
      wifi_hotspot_b->Connect(service_id, {service_address}, &flag);
  EXPECT_TRUE(channel_result.has_error());

  channel_result = wifi_hotspot_b->Connect(
      service_id, hotspot_credentials->GetAddressCandidates(), &flag);
  EXPECT_TRUE(channel_result.has_value());
  EXPECT_TRUE(channel_result.value());

  EXPECT_TRUE(wifi_hotspot_b->DisconnectWifiHotspot());
  EXPECT_TRUE(wifi_hotspot_a->StopWifiHotspot());
}

TEST_P(WifiHotspotTest, CanStartHotspotThatOtherCanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);

  std::string service_id(kServiceID);
  auto wifi_hotspot_a = std::make_unique<WifiHotspot>();
  auto wifi_hotspot_b = std::make_unique<WifiHotspot>();

  EXPECT_TRUE(wifi_hotspot_a->StartWifiHotspot());
  if (!wifi_hotspot_a->IsAcceptingConnections(service_id)) {
    EXPECT_TRUE(wifi_hotspot_a->StartAcceptingConnections(service_id, {}));
  }

  HotspotCredentials* hotspot_credentials =
      wifi_hotspot_a->GetCredentials(service_id);

  EXPECT_TRUE(wifi_hotspot_b->ConnectWifiHotspot(*hotspot_credentials));

  CancellationFlag flag(true);
  ErrorOr<std::unique_ptr<EndpointChannel>> channel_result =
      wifi_hotspot_b->Connect(
          service_id, hotspot_credentials->GetAddressCandidates(), &flag);

  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(channel_result.has_value());
    EXPECT_TRUE(channel_result.value());
    EXPECT_TRUE(wifi_hotspot_b->DisconnectWifiHotspot());
    EXPECT_TRUE(wifi_hotspot_a->StopWifiHotspot());
  } else {
    EXPECT_TRUE(channel_result.has_error());
    EXPECT_TRUE(wifi_hotspot_b->DisconnectWifiHotspot());
    EXPECT_TRUE(wifi_hotspot_a->StopWifiHotspot());
  }
}

TEST_F(WifiHotspotTest, CanStartHotspotTheOtherFailConnect) {
  auto wifi_hotspot_a = std::make_unique<WifiHotspot>();
  auto wifi_hotspot_b = std::make_unique<WifiHotspot>();

  EXPECT_TRUE(wifi_hotspot_a->StartWifiHotspot());

  HotspotCredentials hotspot_credentials;
  hotspot_credentials.SetSSID(std::string(kSsid));
  hotspot_credentials.SetPassword(std::string(kPassword));
  EXPECT_FALSE(wifi_hotspot_b->ConnectWifiHotspot(hotspot_credentials));
  EXPECT_TRUE(wifi_hotspot_b->DisconnectWifiHotspot());

  EXPECT_TRUE(wifi_hotspot_a->StopWifiHotspot());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
