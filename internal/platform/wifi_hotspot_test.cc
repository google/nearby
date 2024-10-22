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

#include "internal/platform/wifi_hotspot.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/wifi_credential.h"

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

constexpr absl::string_view kSsid = "Direct-357a2d8c";
constexpr absl::string_view kPassword = "b592f7d3";
constexpr absl::string_view kIp = "123.234.23.1";
constexpr const size_t kPort = 20;
constexpr int kFrequency = 2412;
constexpr absl::string_view kData = "ABCD";
constexpr const size_t kChunkSize = 10;

TEST(HotspotCredentialsTest, SetGetSsid) {
  std::string ssid(kSsid);
  HotspotCredentials hotspot_credentials;
  hotspot_credentials.SetSSID(ssid);

  EXPECT_EQ(hotspot_credentials.GetSSID(), kSsid);
}

TEST(HotspotCredentialsTest, SetGetPassword) {
  std::string password(kPassword);
  HotspotCredentials hotspot_credentials;
  hotspot_credentials.SetPassword(password);

  EXPECT_EQ(hotspot_credentials.GetPassword(), kPassword);
}

class WifiHotspotMediumTest : public testing::TestWithParam<FeatureFlags> {
 protected:
  WifiHotspotMediumTest() {
    env_.Stop();
    env_.Start();
  }
  ~WifiHotspotMediumTest() override { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

INSTANTIATE_TEST_SUITE_P(ParametrisedWifiHotspotMediumTest,
                         WifiHotspotMediumTest, testing::ValuesIn(kTestCases));

TEST_F(WifiHotspotMediumTest, ConstructorDestructorWorks) {
  auto wifi_hotspot_a = std::make_unique<WifiHotspotMedium>();
  auto wifi_hotspot_b = std::make_unique<WifiHotspotMedium>();

  // Make sure we can create functional mediums.
  ASSERT_TRUE(wifi_hotspot_a->IsInterfaceValid());
  ASSERT_TRUE(wifi_hotspot_b->IsInterfaceValid());

  // Make sure we can create 2 distinct mediums.
  EXPECT_NE(&wifi_hotspot_a->GetImpl(), &wifi_hotspot_b->GetImpl());
  wifi_hotspot_a.reset();
  wifi_hotspot_b.reset();
}

TEST_F(WifiHotspotMediumTest, CanStartStopHotspot) {
  auto wifi_hotspot_a = std::make_unique<WifiHotspotMedium>();

  ASSERT_TRUE(wifi_hotspot_a->IsInterfaceValid());
  EXPECT_TRUE(wifi_hotspot_a->StartWifiHotspot());
  EXPECT_EQ(wifi_hotspot_a->GetDynamicPortRange(), std::nullopt);
  WifiHotspotServerSocket server_socket = wifi_hotspot_a->ListenForService();
  EXPECT_TRUE(server_socket.IsValid());
  server_socket.Close();
  EXPECT_TRUE(wifi_hotspot_a->StopWifiHotspot());
  wifi_hotspot_a.reset();
}

TEST_F(WifiHotspotMediumTest, CanConnectDisconnectHotspot) {
  auto wifi_hotspot_a = std::make_unique<WifiHotspotMedium>();
  std::string ssid(kSsid);
  std::string password(kPassword);

  ASSERT_TRUE(wifi_hotspot_a->IsInterfaceValid());
  EXPECT_FALSE(wifi_hotspot_a->ConnectWifiHotspot(ssid, password, kFrequency));
  EXPECT_TRUE(wifi_hotspot_a->DisconnectWifiHotspot());
  wifi_hotspot_a.reset();
}

TEST_P(WifiHotspotMediumTest, CanStartHotspotThatOtherConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  auto wifi_hotspot_a = std::make_unique<WifiHotspotMedium>();
  auto wifi_hotspot_b = std::make_unique<WifiHotspotMedium>();

  ASSERT_TRUE(wifi_hotspot_a->IsInterfaceValid());
  ASSERT_TRUE(wifi_hotspot_b->IsInterfaceValid());
  EXPECT_TRUE(wifi_hotspot_a->StartWifiHotspot());
  HotspotCredentials* hotspot_credentials = wifi_hotspot_a->GetCredential();
  EXPECT_TRUE(wifi_hotspot_b->ConnectWifiHotspot(
      hotspot_credentials->GetSSID(), hotspot_credentials->GetPassword(),
      hotspot_credentials->GetFrequency()));

  WifiHotspotServerSocket server_socket = wifi_hotspot_a->ListenForService();
  EXPECT_TRUE(server_socket.IsValid());
  wifi_hotspot_a->GetCredential()->SetIPAddress(server_socket.GetIPAddress());

  WifiHotspotSocket socket_a;
  WifiHotspotSocket socket_b;
  EXPECT_FALSE(socket_a.IsValid());
  EXPECT_FALSE(socket_b.IsValid());

  {
    CancellationFlag flag;
    SingleThreadExecutor server_executor;
    SingleThreadExecutor client_executor;
    client_executor.Execute(
        [&wifi_hotspot_b, &socket_b, &server_socket, &flag]() {
          socket_b = wifi_hotspot_b->ConnectToService(kIp, kPort, &flag);
          EXPECT_FALSE(socket_b.IsValid());
          socket_b = wifi_hotspot_b->ConnectToService(
              server_socket.GetIPAddress(), server_socket.GetPort(), &flag);
          if (!socket_b.IsValid()) {
            server_socket.Close();
          }
        });
    server_executor.Execute([&socket_a, &server_socket]() {
      socket_a = server_socket.Accept();
      if (!socket_a.IsValid()) {
        server_socket.Close();
      }
    });
  }
  EXPECT_TRUE(socket_a.IsValid());
  EXPECT_TRUE(socket_b.IsValid());
  InputStream& in_stream = socket_a.GetInputStream();
  OutputStream& out_stream = socket_b.GetOutputStream();
  std::string data(kData);
  EXPECT_TRUE(out_stream.Write(ByteArray(data)).Ok());
  ExceptionOr<ByteArray> read_data = in_stream.Read(kChunkSize);
  EXPECT_TRUE(read_data.ok());
  EXPECT_EQ(std::string(read_data.result()), data);

  socket_a.Close();
  socket_b.Close();
  EXPECT_FALSE(out_stream.Write(ByteArray(data)).Ok());
  read_data = in_stream.Read(kChunkSize);
  EXPECT_TRUE(read_data.GetResult().Empty());

  server_socket.Close();
  EXPECT_TRUE(wifi_hotspot_b->DisconnectWifiHotspot());
  EXPECT_TRUE(wifi_hotspot_a->StopWifiHotspot());
  wifi_hotspot_a.reset();
  wifi_hotspot_b.reset();
}

TEST_P(WifiHotspotMediumTest, CanStartHotspotThatOtherCanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  auto wifi_hotspot_a = std::make_unique<WifiHotspotMedium>();
  auto wifi_hotspot_b = std::make_unique<WifiHotspotMedium>();

  ASSERT_TRUE(wifi_hotspot_a->IsInterfaceValid());
  ASSERT_TRUE(wifi_hotspot_b->IsInterfaceValid());
  EXPECT_TRUE(wifi_hotspot_a->StartWifiHotspot());
  HotspotCredentials* hotspot_credentials = wifi_hotspot_a->GetCredential();
  EXPECT_TRUE(wifi_hotspot_b->ConnectWifiHotspot(
      hotspot_credentials->GetSSID(), hotspot_credentials->GetPassword(),
      hotspot_credentials->GetFrequency()));

  WifiHotspotServerSocket server_socket = wifi_hotspot_a->ListenForService();
  EXPECT_TRUE(server_socket.IsValid());
  wifi_hotspot_a->GetCredential()->SetIPAddress(server_socket.GetIPAddress());

  WifiHotspotSocket socket_a;
  WifiHotspotSocket socket_b;
  EXPECT_FALSE(socket_a.IsValid());
  EXPECT_FALSE(socket_b.IsValid());

  {
    CancellationFlag flag(true);
    SingleThreadExecutor server_executor;
    SingleThreadExecutor client_executor;
    client_executor.Execute(
        [&wifi_hotspot_b, &socket_b, &server_socket, &flag]() {
          socket_b = wifi_hotspot_b->ConnectToService(kIp, kPort, &flag);
          EXPECT_FALSE(socket_b.IsValid());
          socket_b = wifi_hotspot_b->ConnectToService(
              server_socket.GetIPAddress(), server_socket.GetPort(), &flag);
          if (!socket_b.IsValid()) {
            server_socket.Close();
          }
        });
    server_executor.Execute([&socket_a, &server_socket]() {
      socket_a = server_socket.Accept();
      if (!socket_a.IsValid()) {
        server_socket.Close();
      }
    });
  }

  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(socket_a.IsValid());
    EXPECT_TRUE(socket_b.IsValid());
  } else {
    EXPECT_FALSE(socket_a.IsValid());
    EXPECT_FALSE(socket_b.IsValid());
  }

  server_socket.Close();
  EXPECT_TRUE(wifi_hotspot_b->DisconnectWifiHotspot());
  EXPECT_TRUE(wifi_hotspot_a->StopWifiHotspot());
  wifi_hotspot_a.reset();
  wifi_hotspot_b.reset();
}

TEST_F(WifiHotspotMediumTest, CanStartHotspotTheOtherFailConnect) {
  auto wifi_hotspot_a = std::make_unique<WifiHotspotMedium>();
  auto wifi_hotspot_b = std::make_unique<WifiHotspotMedium>();

  ASSERT_TRUE(wifi_hotspot_a->IsInterfaceValid());
  ASSERT_TRUE(wifi_hotspot_b->IsInterfaceValid());
  EXPECT_TRUE(wifi_hotspot_a->StartWifiHotspot());

  std::string ssid(kSsid);
  std::string password(kPassword);

  EXPECT_FALSE(wifi_hotspot_b->ConnectWifiHotspot(ssid, password, kFrequency));
  EXPECT_TRUE(wifi_hotspot_b->DisconnectWifiHotspot());

  EXPECT_TRUE(wifi_hotspot_a->StopWifiHotspot());
  wifi_hotspot_a.reset();
  wifi_hotspot_b.reset();
}

}  // namespace
}  // namespace nearby
