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
#include "absl/strings/match.h"
#include "internal/platform/medium_environment.h"

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
constexpr absl::Duration kWaitDuration = absl::Milliseconds(100);

class WifiDirectMediumTest : public testing::TestWithParam<FeatureFlags> {
 protected:
  WifiDirectMediumTest() { env_.Start(); }
  ~WifiDirectMediumTest() override {
    absl::SleepFor(kWaitDuration);
    EXPECT_TRUE(env_.IsWifiDirectMediumsEmpty());
    env_.Stop();
  }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

INSTANTIATE_TEST_SUITE_P(ParametrisedWifiDirectMediumTest, WifiDirectMediumTest,
                         testing::ValuesIn(kTestCases));

TEST_F(WifiDirectMediumTest, ConstructorDestructorWorks) {
  WifiDirectMedium wifi_direct_a;
  WifiDirectMedium wifi_direct_b;

  // Make sure we can create functional mediums.
  ASSERT_TRUE(wifi_direct_a.IsInterfaceValid());
  ASSERT_TRUE(wifi_direct_b.IsInterfaceValid());

  // Make sure we can create 2 distinct mediums.
  EXPECT_NE(&wifi_direct_a.GetImpl(), &wifi_direct_b.GetImpl());
}

TEST_F(WifiDirectMediumTest, CanStartStopDirect) {
  WifiDirectMedium wifi_direct_a;

  ASSERT_TRUE(wifi_direct_a.IsInterfaceValid());
  EXPECT_TRUE(wifi_direct_a.StartWifiDirect());
  EXPECT_EQ(wifi_direct_a.GetDynamicPortRange(), std::nullopt);
  WifiDirectServerSocket server_socket = wifi_direct_a.ListenForService();
  EXPECT_TRUE(server_socket.IsValid());
  server_socket.Close();
  EXPECT_TRUE(wifi_direct_a.StopWifiDirect());
}

TEST_F(WifiDirectMediumTest, CanConnectDisconnectDirect) {
  WifiDirectMedium wifi_direct_a;

  ASSERT_TRUE(wifi_direct_a.IsInterfaceValid());
  EXPECT_FALSE(wifi_direct_a.ConnectWifiDirect(kSsid, kPassword));
  EXPECT_TRUE(wifi_direct_a.DisconnectWifiDirect());
}

TEST_P(WifiDirectMediumTest, CanStartDirectGOThatOtherCanConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  WifiDirectMedium wifi_direct_a;
  WifiDirectMedium wifi_direct_b;

  ASSERT_TRUE(wifi_direct_a.IsInterfaceValid());
  ASSERT_TRUE(wifi_direct_b.IsInterfaceValid());
  EXPECT_TRUE(wifi_direct_a.StartWifiDirect());
  WifiDirectCredentials* wifi_direct_credentials =
      wifi_direct_a.GetCredential();
  auto* medium_a =
      env_.GetWifiDirectMedium(wifi_direct_credentials->GetSSID(), {});
  EXPECT_NE(medium_a, nullptr);
  EXPECT_TRUE(
      wifi_direct_b.ConnectWifiDirect(wifi_direct_credentials->GetSSID(),
                                      wifi_direct_credentials->GetPassword()));

  WifiDirectServerSocket server_socket = wifi_direct_a.ListenForService();
  EXPECT_TRUE(server_socket.IsValid());
  auto ip_addr = server_socket.GetIPAddress();
  EXPECT_FALSE(absl::EndsWith(ip_addr, "."));
  wifi_direct_credentials->SetIPAddress(ip_addr);

  WifiDirectSocket socket_a;
  WifiDirectSocket socket_b;
  EXPECT_FALSE(socket_a.IsValid());
  EXPECT_FALSE(socket_b.IsValid());

  {
    CancellationFlag flag;
    SingleThreadExecutor server_executor;
    SingleThreadExecutor client_executor;
    client_executor.Execute(
        [&wifi_direct_b, &socket_b, &server_socket, &flag]() {
          socket_b = wifi_direct_b.ConnectToService(kIp, kPort, &flag);
          EXPECT_FALSE(socket_b.IsValid());
          socket_b = wifi_direct_b.ConnectToService(
              server_socket.GetIPAddress(), kPort, &flag);
          EXPECT_FALSE(socket_b.IsValid());
          socket_b = wifi_direct_b.ConnectToService(
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
  EXPECT_TRUE(wifi_direct_b.DisconnectWifiDirect());
  EXPECT_TRUE(wifi_direct_a.StopWifiDirect());
  auto* medium_b =
      env_.GetWifiDirectMedium(wifi_direct_credentials->GetSSID(), {});
  EXPECT_EQ(medium_b, nullptr);
}

TEST_P(WifiDirectMediumTest, CanStartDirectGOThatOtherCanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  WifiDirectMedium wifi_direct_a;
  WifiDirectMedium wifi_direct_b;

  ASSERT_TRUE(wifi_direct_a.IsInterfaceValid());
  ASSERT_TRUE(wifi_direct_b.IsInterfaceValid());
  EXPECT_TRUE(wifi_direct_a.StartWifiDirect());
  WifiDirectCredentials* wifi_direct_credentials =
      wifi_direct_a.GetCredential();
  EXPECT_TRUE(
      wifi_direct_b.ConnectWifiDirect(wifi_direct_credentials->GetSSID(),
                                      wifi_direct_credentials->GetPassword()));

  WifiDirectServerSocket server_socket = wifi_direct_a.ListenForService();
  EXPECT_TRUE(server_socket.IsValid());
  wifi_direct_credentials->SetIPAddress(server_socket.GetIPAddress());

  WifiDirectSocket socket_a;
  WifiDirectSocket socket_b;
  WifiDirectSocket socket_c;
  EXPECT_FALSE(socket_a.IsValid());
  EXPECT_FALSE(socket_b.IsValid());

  {
    CancellationFlag flag(true);
    SingleThreadExecutor server_executor;
    SingleThreadExecutor client_executor;
    client_executor.Execute(
        [&wifi_direct_b, &socket_b, &server_socket, &flag]() {
          socket_b = wifi_direct_b.ConnectToService(kIp, kPort, &flag);
          EXPECT_FALSE(socket_b.IsValid());
          socket_b = wifi_direct_b.ConnectToService(
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
  {
    CancellationFlag flag(true);
    socket_c = wifi_direct_b.ConnectToService(server_socket.GetIPAddress(),
                                              server_socket.GetPort(), &flag);
    EXPECT_FALSE(socket_c.IsValid());
  }

  EXPECT_TRUE(wifi_direct_b.DisconnectWifiDirect());
  EXPECT_TRUE(wifi_direct_a.StopWifiDirect());
}

TEST_F(WifiDirectMediumTest, CanStartDirectGOThatOtherFailConnect) {
  WifiDirectMedium wifi_direct_a;
  WifiDirectMedium wifi_direct_b;

  ASSERT_TRUE(wifi_direct_a.IsInterfaceValid());
  ASSERT_TRUE(wifi_direct_b.IsInterfaceValid());
  EXPECT_TRUE(wifi_direct_a.StartWifiDirect());

  EXPECT_FALSE(wifi_direct_b.ConnectWifiDirect(kSsid, kPassword));
  EXPECT_TRUE(wifi_direct_b.DisconnectWifiDirect());

  EXPECT_TRUE(wifi_direct_a.StopWifiDirect());
}

}  // namespace
}  // namespace nearby
