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
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/wifi_direct_bwu_handler.h"
#include "internal/flags/nearby_flags.h"
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
using ::location::nearby::proto::connections::OperationResultCode;
constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kServiceID{
    "com.google.location.nearby.connections.test"};
constexpr absl::string_view kEndpointID{"WifiDirect_GO"};
}  // namespace

class WifiDirectTest : public testing::Test {
 protected:
  WifiDirectTest() {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        connections::config_package_nearby::nearby_connections_feature::
            kEnableWifiDirect,
        true);
    env_.Start();
  }
  ~WifiDirectTest() override { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(WifiDirectTest, CanCreateBwuHandler) {
  ClientProxy client;
  Mediums mediums;

  auto handler = std::make_unique<WifiDirectBwuHandler>(mediums, nullptr);

  handler->InitializeUpgradedMediumForEndpoint(&client, std::string(kServiceID),
                                               std::string(kEndpointID));
  handler->RevertInitiatorState();
  SUCCEED();
  handler.reset();
}

TEST_F(WifiDirectTest, WFDGOBWUInit_GCCreateEndpointChannel) {
  CountDownLatch start_latch(1);
  CountDownLatch accept_latch(1);
  CountDownLatch end_latch(1);

  ClientProxy wifi_direct_go, wifi_direct_gc;
  Mediums mediums_wfd_go, mediums_wfd_gc;
  ExceptionOr<OfflineFrame> upgrade_frame;

  auto wfd_go_bwu_handler = std::make_unique<WifiDirectBwuHandler>(
      mediums_wfd_go, [&](ClientProxy* client,
                     std::unique_ptr<BwuHandler::IncomingSocketConnection>
                         mutable_connection) {
        LOG(INFO) << "Server socket connection accept call back, Socket name: "
                  << mutable_connection->socket->ToString();
        accept_latch.CountDown();
        EXPECT_TRUE(end_latch.Await(kWaitDuration).result());
      });

  SingleThreadExecutor wfd_go_executor;
  wfd_go_executor.Execute([&]() {
    ByteArray upgrade_path_available_frame =
        wfd_go_bwu_handler->InitializeUpgradedMediumForEndpoint(
            &wifi_direct_go, std::string(kServiceID), std::string(kEndpointID));
    EXPECT_FALSE(upgrade_path_available_frame.Empty());

    upgrade_frame = parser::FromBytes(upgrade_path_available_frame);
    start_latch.CountDown();
  });

  wifi_direct_gc.AddCancellationFlag(std::string(kEndpointID));
  SingleThreadExecutor wfd_gc_executor;
  // Wait till wifi_direct_go started and then connect to it
  EXPECT_TRUE(start_latch.Await(kWaitDuration).result());
  EXPECT_FALSE(mediums_wfd_gc.GetWifiDirect().IsConnectedToGO());
  std::unique_ptr<BwuHandler> wfd_gc_bwu_handler =
      std::make_unique<WifiDirectBwuHandler>(mediums_wfd_gc, nullptr);

  wfd_gc_executor.Execute([&]() {
    UpgradePathInfo upgrade_path_info;
    ErrorOr<std::unique_ptr<EndpointChannel>> result_error =
        wfd_gc_bwu_handler->CreateUpgradedEndpointChannel(
            &wifi_direct_gc, std::string(kServiceID),
            std::string(kEndpointID), upgrade_path_info);
    EXPECT_TRUE(result_error.has_error());

    auto bwu_frame =
        upgrade_frame.result().v1().bandwidth_upgrade_negotiation();

    ErrorOr<std::unique_ptr<EndpointChannel>> result =
        wfd_gc_bwu_handler->CreateUpgradedEndpointChannel(
            &wifi_direct_gc, std::string(kServiceID),
            std::string(kEndpointID), bwu_frame.upgrade_path_info());
    EXPECT_EQ(wfd_gc_bwu_handler->GetUpgradeMedium(),
              location::nearby::proto::connections::Medium::WIFI_DIRECT);

    if (!wifi_direct_gc.GetCancellationFlag(std::string(kEndpointID))
             ->Cancelled()) {
      ASSERT_TRUE(result.has_value());
      std::unique_ptr<EndpointChannel> new_channel = std::move(result.value());
      EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
      EXPECT_EQ(new_channel->GetMedium(),
                location::nearby::proto::connections::Medium::WIFI_DIRECT);
      absl::SleepFor(absl::Milliseconds(100));
      new_channel->Close();
    } else {
      EXPECT_FALSE(result.has_value());
      EXPECT_TRUE(result.has_error());
      EXPECT_EQ(result.error().operation_result_code(),
                OperationResultCode::
                    CLIENT_CANCELLATION_CANCEL_WIFI_DIRECT_OUTGOING_CONNECTION);
      accept_latch.CountDown();
    }
    EXPECT_TRUE(mediums_wfd_gc.GetWifiDirect().IsConnectedToGO());
    wfd_gc_bwu_handler->RevertResponderState(std::string(kServiceID));
    end_latch.CountDown();
  });
  wfd_gc_bwu_handler->OnEndpointDisconnect(&wifi_direct_gc,
                                  std::string(kEndpointID));

  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(end_latch.Await(kWaitDuration).result());
  EXPECT_FALSE(mediums_wfd_gc.GetWifiDirect().IsConnectedToGO());
}

}  // namespace connections
}  // namespace nearby
