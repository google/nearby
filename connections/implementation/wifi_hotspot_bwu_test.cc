// Copyright 2022 Google LLC
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
#include "connections/implementation/wifi_hotspot_bwu_handler.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/expected.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::connections::OfflineFrame;
using UpgradePathInfo = ::location::nearby::connections::
    BandwidthUpgradeNegotiationFrame::UpgradePathInfo;
using ::location::nearby::proto::connections::OperationResultCode;
constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kEndpointID{"Hotspot_Server"};
}  // namespace

class WifiHotspotTest : public testing::Test {
 protected:
  WifiHotspotTest() { env_.Start(); }
  ~WifiHotspotTest() override { env_.Stop(); }
  void SetUp() override {
    nearby::NearbyFlags::GetInstance().OverrideInt64FlagValue(
      platform::config_package_nearby::nearby_platform_feature::
              kWifiHotspotConnectionIntervalMillis, 1);
  }
  void TearDown() override {
    nearby::NearbyFlags::GetInstance().ResetOverridedValues();
  }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(WifiHotspotTest, CanCreateBwuHandler) {
  ClientProxy client;
  Mediums mediums;

  auto handler = std::make_unique<WifiHotspotBwuHandler>(mediums, nullptr);

  handler->InitializeUpgradedMediumForEndpoint(&client, std::string(kServiceID),
                                               std::string(kEndpointID));
  handler->RevertInitiatorState();
  SUCCEED();
  handler.reset();
}

TEST_F(WifiHotspotTest, SoftAPBWUInit_STACreateEndpointChannel) {
  CountDownLatch start_latch(1);
  CountDownLatch accept_latch(1);
  CountDownLatch end_latch(1);

  ClientProxy client_hotspot_ap, client_hotspot_sta;
  Mediums mediums_HS_ap, mediums_HS_sta;
  ExceptionOr<OfflineFrame> upgrade_frame;

  auto handler_1 = std::make_unique<WifiHotspotBwuHandler>(
      mediums_HS_ap, [&](ClientProxy* client,
                         std::unique_ptr<BwuHandler::IncomingSocketConnection>
                             mutable_connection) {
        LOG(INFO) << "Server socket connection accept call back, Socket name: "
                  << mutable_connection->socket->ToString();
        accept_latch.CountDown();
        EXPECT_TRUE(end_latch.Await(kWaitDuration).result());
      });

  // client_hotspot_ap works as Hotspot SoftAP
  SingleThreadExecutor server_executor;
  server_executor.Execute([&]() {
    ByteArray upgrade_path_available_frame =
        handler_1->InitializeUpgradedMediumForEndpoint(
            &client_hotspot_ap, std::string(kServiceID),
            std::string(kEndpointID));
    EXPECT_FALSE(upgrade_path_available_frame.Empty());

    upgrade_frame = parser::FromBytes(upgrade_path_available_frame);
    start_latch.CountDown();
  });

  client_hotspot_sta.AddCancellationFlag(std::string(kEndpointID));
  // client_hotspot_sta works as Hotspot STA which will connect to
  // client_hotspot_ap
  SingleThreadExecutor client_executor;
  // Wait till client_hotspot_ap started as hotspot and then connect to it
  EXPECT_TRUE(start_latch.Await(kWaitDuration).result());
  std::unique_ptr<BwuHandler> handler_2 =
      std::make_unique<WifiHotspotBwuHandler>(mediums_HS_sta, nullptr);

  client_executor.Execute([&]() {
    UpgradePathInfo upgrade_path_info;
    ErrorOr<std::unique_ptr<EndpointChannel>> result_error =
        handler_2->CreateUpgradedEndpointChannel(
            &client_hotspot_sta, std::string(kServiceID),
            std::string(kEndpointID), upgrade_path_info);
    EXPECT_TRUE(result_error.has_error());

    auto bwu_frame =
        upgrade_frame.result().v1().bandwidth_upgrade_negotiation();

    ErrorOr<std::unique_ptr<EndpointChannel>> result =
        handler_2->CreateUpgradedEndpointChannel(
            &client_hotspot_sta, std::string(kServiceID),
            std::string(kEndpointID), bwu_frame.upgrade_path_info());
    EXPECT_EQ(handler_2->GetUpgradeMedium(),
              location::nearby::proto::connections::Medium::WIFI_HOTSPOT);

    if (!client_hotspot_sta.GetCancellationFlag(std::string(kEndpointID))
             ->Cancelled()) {
      ASSERT_TRUE(result.has_value());
      std::unique_ptr<EndpointChannel> new_channel = std::move(result.value());
      EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
      EXPECT_EQ(new_channel->GetMedium(),
                location::nearby::proto::connections::Medium::WIFI_HOTSPOT);
      absl::SleepFor(absl::Milliseconds(100));
      new_channel->Close();
    } else {
      EXPECT_FALSE(result.has_value());
      EXPECT_TRUE(result.has_error());
      EXPECT_EQ(
          result.error().operation_result_code(),
          OperationResultCode::
              CLIENT_CANCELLATION_CANCEL_WIFI_HOTSPOT_OUTGOING_CONNECTION);
      accept_latch.CountDown();
    }
    EXPECT_TRUE(mediums_HS_sta.GetWifiHotspot().IsConnectedToHotspot());
    handler_2->RevertResponderState(std::string(kServiceID));
    end_latch.CountDown();
  });
  handler_2->OnEndpointDisconnect(&client_hotspot_sta,
                                  std::string(kEndpointID));

  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(end_latch.Await(kWaitDuration).result());
  EXPECT_FALSE(mediums_HS_sta.GetWifiHotspot().IsConnectedToHotspot());
}

}  // namespace connections
}  // namespace nearby
