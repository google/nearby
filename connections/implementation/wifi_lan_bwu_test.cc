// Copyright 2025 Google LLC
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
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/wifi_lan_bwu_handler.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/expected.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::connections::OfflineFrame;
using UpgradePathInfo = ::location::nearby::connections::
    BandwidthUpgradeNegotiationFrame::UpgradePathInfo;
constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kEndpointID{"WifiLan_Server"};
}  // namespace

class WifiLanBwuTest : public testing::Test {
 protected:
  WifiLanBwuTest() { env_.Start(); }
  ~WifiLanBwuTest() override { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(WifiLanBwuTest, CanCreateBwuHandler) {
  ClientProxy client;
  Mediums mediums;

  auto handler = std::make_unique<WifiLanBwuHandler>(mediums, nullptr);

  handler->InitializeUpgradedMediumForEndpoint(&client, std::string(kServiceID),
                                               std::string(kEndpointID));
  handler->RevertInitiatorState();
  SUCCEED();
  handler.reset();
}

TEST_F(WifiLanBwuTest, WifiLanBWUInit_ClientCreateEndpointChannel) {
  CountDownLatch start_latch(1);
  CountDownLatch accept_latch(1);
  CountDownLatch end_latch(1);

  ClientProxy client_wlan_server, client_wlan_client;
  Mediums mediums_lan_ap, mediums_lan_sta;
  ExceptionOr<OfflineFrame> upgrade_frame;

  auto handler_server = std::make_unique<WifiLanBwuHandler>(
      mediums_lan_ap, [&](ClientProxy* client,
                          std::unique_ptr<BwuHandler::IncomingSocketConnection>
                              mutable_connection) {
        LOG(INFO) << "Server socket connection accept call back, Socket name: "
                  << mutable_connection->socket->ToString();
        accept_latch.CountDown();
        EXPECT_TRUE(end_latch.Await(kWaitDuration).result());
      });

  SingleThreadExecutor server_executor;
  server_executor.Execute([&]() {
    ByteArray upgrade_path_available_frame =
        handler_server->InitializeUpgradedMediumForEndpoint(
            &client_wlan_server, std::string(kServiceID),
            std::string(kEndpointID));
    EXPECT_FALSE(upgrade_path_available_frame.Empty());

    upgrade_frame = parser::FromBytes(upgrade_path_available_frame);
    start_latch.CountDown();
  });

  client_wlan_client.AddCancellationFlag(std::string(kEndpointID));
  SingleThreadExecutor client_executor;
  // Wait till client_wlan_server started as hotspot and then connect to it
  EXPECT_TRUE(start_latch.Await(kWaitDuration).result());
  std::unique_ptr<BwuHandler> handler_client =
      std::make_unique<WifiLanBwuHandler>(mediums_lan_sta, nullptr);

  client_executor.Execute([&]() {
    // Fail case 1:
    UpgradePathInfo upgrade_path_info;
    ErrorOr<std::unique_ptr<EndpointChannel>> result_error =
        handler_client->CreateUpgradedEndpointChannel(
            &client_wlan_client, std::string(kServiceID),
            std::string(kEndpointID), upgrade_path_info);
    EXPECT_TRUE(result_error.has_error());

    // Fail case 2:
    upgrade_path_info.set_medium(UpgradePathInfo::WIFI_LAN);
    upgrade_path_info.mutable_wifi_lan_socket()->set_ip_address("192.168.1.1");
    result_error = handler_client->CreateUpgradedEndpointChannel(
        &client_wlan_client, std::string(kServiceID), std::string(kEndpointID),
        upgrade_path_info);
    EXPECT_TRUE(result_error.has_error());

    // Fail case 3:
    upgrade_path_info.mutable_wifi_lan_socket()->set_wifi_port(12345);
    result_error = handler_client->CreateUpgradedEndpointChannel(
        &client_wlan_client, std::string(kServiceID), std::string(kEndpointID),
        upgrade_path_info);
    EXPECT_TRUE(result_error.has_error());

    // Success case:
    auto bwu_frame =
        upgrade_frame.result().v1().bandwidth_upgrade_negotiation();

    ErrorOr<std::unique_ptr<EndpointChannel>> result =
        handler_client->CreateUpgradedEndpointChannel(
            &client_wlan_client, std::string(kServiceID),
            std::string(kEndpointID), bwu_frame.upgrade_path_info());
    EXPECT_EQ(handler_client->GetUpgradeMedium(),
              location::nearby::proto::connections::Medium::WIFI_LAN);

    if (!client_wlan_client.GetCancellationFlag(std::string(kEndpointID))
             ->Cancelled()) {
      ASSERT_TRUE(result.has_value());
      std::unique_ptr<EndpointChannel> new_channel = std::move(result.value());
      EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
      EXPECT_EQ(new_channel->GetMedium(),
                location::nearby::proto::connections::Medium::WIFI_LAN);
      new_channel->EnableMultiplexSocket();
      absl::SleepFor(absl::Milliseconds(100));
      new_channel->Close();
    } else {
      EXPECT_FALSE(result.has_value());
      EXPECT_TRUE(result.has_error());
      accept_latch.CountDown();
    }
    handler_client->RevertResponderState(std::string(kServiceID));
    end_latch.CountDown();
  });
  handler_client->OnEndpointDisconnect(&client_wlan_client,
                                       std::string(kEndpointID));

  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(end_latch.Await(kWaitDuration).result());
}

}  // namespace connections
}  // namespace nearby
