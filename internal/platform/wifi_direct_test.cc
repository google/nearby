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
#include "internal/platform/medium_environment.h"
#include "internal/platform/wifi_hotspot.h"
#include "internal/platform/wifi_hotspot_credential.h"

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

constexpr absl::string_view kSsid = "Direct-357a2d8c";
constexpr absl::string_view kPassword = "b592f7d3";
constexpr absl::string_view kIp = "123.234.23.1";
constexpr const size_t kPort = 20;
constexpr absl::string_view kData = "ABCD";
constexpr const size_t kChunkSize = 10;

class WifiDirectMediumTest : public testing::TestWithParam<FeatureFlags> {
 protected:
  WifiDirectMediumTest() {
    env_.Stop();
    env_.Start();
  }
  ~WifiDirectMediumTest() override { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

INSTANTIATE_TEST_SUITE_P(ParametrisedWifiDirectMediumTest, WifiDirectMediumTest,
                         testing::ValuesIn(kTestCases));

TEST_F(WifiDirectMediumTest, ConstructorDestructorWorks) {
  auto wifi_direct_a = std::make_unique<WifiDirectMedium>();
  auto wifi_direct_b = std::make_unique<WifiDirectMedium>();

  // Make sure we can create functional mediums.
  ASSERT_TRUE(wifi_direct_a->IsInterfaceValid());
  ASSERT_TRUE(wifi_direct_b->IsInterfaceValid());

  // Make sure we can create 2 distinct mediums.
  EXPECT_NE(&wifi_direct_a->GetImpl(), &wifi_direct_b->GetImpl());
  wifi_direct_a.reset();
  wifi_direct_b.reset();
}

TEST_F(WifiDirectMediumTest, CanStartStopDirect) {
  auto wifi_direct_a = std::make_unique<WifiDirectMedium>();

  ASSERT_TRUE(wifi_direct_a->IsInterfaceValid());
  EXPECT_TRUE(wifi_direct_a->StartWifiDirect());
  EXPECT_EQ(wifi_direct_a->GetDynamicPortRange(), std::nullopt);
  WifiHotspotServerSocket server_socket = wifi_direct_a->ListenForService();
  EXPECT_TRUE(server_socket.IsValid());
  server_socket.Close();
  EXPECT_TRUE(wifi_direct_a->StopWifiDirect());
  wifi_direct_a.reset();
}

TEST_F(WifiDirectMediumTest, CanConnectDisconnectDirect) {
  auto wifi_direct_a = std::make_unique<WifiDirectMedium>();
  std::string ssid(kSsid);
  std::string password(kPassword);

  ASSERT_TRUE(wifi_direct_a->IsInterfaceValid());
  EXPECT_FALSE(wifi_direct_a->ConnectWifiDirect(ssid, password));
  EXPECT_TRUE(wifi_direct_a->DisconnectWifiDirect());
  wifi_direct_a.reset();
}

TEST_P(WifiDirectMediumTest, CanStartDirectThatOtherConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  auto wifi_direct_a = std::make_unique<WifiDirectMedium>();
  auto wifi_direct_b = std::make_unique<WifiDirectMedium>();

  ASSERT_TRUE(wifi_direct_a->IsInterfaceValid());
  ASSERT_TRUE(wifi_direct_b->IsInterfaceValid());
  EXPECT_TRUE(wifi_direct_a->StartWifiDirect());
  HotspotCredentials* wifi_direct_credentials = wifi_direct_a->GetCredential();
  EXPECT_TRUE(
      wifi_direct_b->ConnectWifiDirect(wifi_direct_credentials->GetSSID(),
                                       wifi_direct_credentials->GetPassword()));

  WifiHotspotServerSocket server_socket = wifi_direct_a->ListenForService();
  EXPECT_TRUE(server_socket.IsValid());
  wifi_direct_a->GetCredential()->SetIPAddress(server_socket.GetIPAddress());

  WifiHotspotSocket socket_a;
  WifiHotspotSocket socket_b;
  EXPECT_FALSE(socket_a.IsValid());
  EXPECT_FALSE(socket_b.IsValid());

  {
    CancellationFlag flag;
    SingleThreadExecutor server_executor;
    SingleThreadExecutor client_executor;
    client_executor.Execute(
        [&wifi_direct_b, &socket_b, &server_socket, &flag]() {
          socket_b = wifi_direct_b->ConnectToService(kIp, kPort, &flag);
          EXPECT_FALSE(socket_b.IsValid());
          socket_b = wifi_direct_b->ConnectToService(
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
  EXPECT_FALSE(read_data.ok());

  server_socket.Close();
  EXPECT_TRUE(wifi_direct_b->DisconnectWifiDirect());
  EXPECT_TRUE(wifi_direct_a->StopWifiDirect());
  wifi_direct_a.reset();
  wifi_direct_b.reset();
}

TEST_P(WifiDirectMediumTest, CanStartDirectThatOtherCanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  auto wifi_direct_a = std::make_unique<WifiDirectMedium>();
  auto wifi_direct_b = std::make_unique<WifiDirectMedium>();

  ASSERT_TRUE(wifi_direct_a->IsInterfaceValid());
  ASSERT_TRUE(wifi_direct_b->IsInterfaceValid());
  EXPECT_TRUE(wifi_direct_a->StartWifiDirect());
  HotspotCredentials* wifi_direct_credentials = wifi_direct_a->GetCredential();
  EXPECT_TRUE(
      wifi_direct_b->ConnectWifiDirect(wifi_direct_credentials->GetSSID(),
                                       wifi_direct_credentials->GetPassword()));

  WifiHotspotServerSocket server_socket = wifi_direct_a->ListenForService();
  EXPECT_TRUE(server_socket.IsValid());
  wifi_direct_a->GetCredential()->SetIPAddress(server_socket.GetIPAddress());

  WifiHotspotSocket socket_a;
  WifiHotspotSocket socket_b;
  EXPECT_FALSE(socket_a.IsValid());
  EXPECT_FALSE(socket_b.IsValid());

  {
    CancellationFlag flag(true);
    SingleThreadExecutor server_executor;
    SingleThreadExecutor client_executor;
    client_executor.Execute(
        [&wifi_direct_b, &socket_b, &server_socket, &flag]() {
          socket_b = wifi_direct_b->ConnectToService(kIp, kPort, &flag);
          EXPECT_FALSE(socket_b.IsValid());
          socket_b = wifi_direct_b->ConnectToService(
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
  EXPECT_TRUE(wifi_direct_b->DisconnectWifiDirect());
  EXPECT_TRUE(wifi_direct_a->StopWifiDirect());
  wifi_direct_a.reset();
  wifi_direct_b.reset();
}

TEST_F(WifiDirectMediumTest, CanStartDirectTheOtherFailConnect) {
  auto wifi_direct_a = std::make_unique<WifiDirectMedium>();
  auto wifi_direct_b = std::make_unique<WifiDirectMedium>();

  ASSERT_TRUE(wifi_direct_a->IsInterfaceValid());
  ASSERT_TRUE(wifi_direct_b->IsInterfaceValid());
  EXPECT_TRUE(wifi_direct_a->StartWifiDirect());

  std::string ssid(kSsid);
  std::string password(kPassword);

  EXPECT_FALSE(wifi_direct_b->ConnectWifiDirect(ssid, password));
  EXPECT_TRUE(wifi_direct_b->DisconnectWifiDirect());

  EXPECT_TRUE(wifi_direct_a->StopWifiDirect());
  wifi_direct_a.reset();
  wifi_direct_b.reset();
}

}  // namespace
}  // namespace nearby
}  // namespace location
