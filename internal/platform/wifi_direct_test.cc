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

#include "internal/platform/wifi_direct.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "internal/platform/wifi_hotspot.h"
#include "internal/platform/wifi_hotspot_credential.h"

namespace location {
namespace nearby {
namespace {
constexpr absl::string_view kSsid = "Direct-357a2d8c";
constexpr absl::string_view kPassword = "b592f7d3";
constexpr absl::string_view kIp = "123.234.23.1";
constexpr const size_t kPort = 20;

class WifiDirectMediumTest : public ::testing::Test {};

TEST_F(WifiDirectMediumTest, ConstructorDestructorWorks) {
  auto wifi_direct_a = WifiDirectMedium();
  auto wifi_direct_b = WifiDirectMedium();

  ASSERT_TRUE(wifi_direct_a.IsInterfaceValid());
  ASSERT_TRUE(wifi_direct_b.IsInterfaceValid());

  // Make sure we can create 2 distinct mediums.
  EXPECT_NE(&wifi_direct_a.GetImpl(), &wifi_direct_b.GetImpl());
}

TEST_F(WifiDirectMediumTest, CanStartStopDirect) {
  auto wifi_direct_a = WifiDirectMedium();
  ASSERT_TRUE(wifi_direct_a.IsInterfaceValid());
  EXPECT_TRUE(wifi_direct_a.StartWifiDirect());
  EXPECT_EQ(wifi_direct_a.GetDynamicPortRange(), std::nullopt);

  WifiHotspotServerSocket server_socket = wifi_direct_a.ListenForService();
  EXPECT_FALSE(server_socket.IsValid());
  EXPECT_TRUE(wifi_direct_a.StopWifiDirect());
}

TEST_F(WifiDirectMediumTest, CanConnectDisconnectDirect) {
  auto wifi_direct_a = WifiDirectMedium();

  ASSERT_TRUE(wifi_direct_a.IsInterfaceValid());
  EXPECT_TRUE(wifi_direct_a.ConnectWifiDirect(kSsid, kPassword));
  EXPECT_TRUE(wifi_direct_a.DisconnectWifiDirect());
}

TEST_F(WifiDirectMediumTest, CanConnectToService) {
  auto wifi_direct = WifiDirectMedium();
  CancellationFlag flag;
  EXPECT_FALSE(wifi_direct.ConnectToService(kIp, kPort, &flag).IsValid());
}
}  // namespace
}  // namespace nearby
}  // namespace location
