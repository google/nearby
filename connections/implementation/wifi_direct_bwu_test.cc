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
#include <utility>

#include "gtest/gtest.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/wifi_direct_bwu_handler.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::connections::OfflineFrame;
constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
}  // namespace

class WifiDirectTest : public testing::Test {
 protected:
  WifiDirectTest() { env_.Start(); }
  ~WifiDirectTest() override { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(WifiDirectTest, CanCreateBwuHandler) {
  ClientProxy client;
  Mediums mediums;

  auto handler = std::make_unique<WifiDirectBwuHandler>(mediums, nullptr);

  handler->InitializeUpgradedMediumForEndpoint(&client, /*service_id=*/"B",
                                               /*endpoint_id=*/"2");
  handler->RevertInitiatorState();
  SUCCEED();
  handler.reset();
}

TEST_F(WifiDirectTest, WFDGOBWUInit_GCCreateEndpointChannel) {
  CountDownLatch start_latch(1);
  CountDownLatch accept_latch(1);
  CountDownLatch end_latch(1);

  ClientProxy wifi_direct_go, wifi_direct_gc;
  Mediums mediums_1, mediums_2;
  ExceptionOr<OfflineFrame> upgrade_frame;

  auto handler_1 = std::make_unique<WifiDirectBwuHandler>(
      mediums_1, [&](ClientProxy* client,
                     std::unique_ptr<BwuHandler::IncomingSocketConnection>
                         mutable_connection) {
        NEARBY_LOGS(WARNING) << "Server socket connection accept call back";
        std::shared_ptr<BwuHandler::IncomingSocketConnection> connection(
            mutable_connection.release());
        accept_latch.CountDown();
        EXPECT_TRUE(end_latch.Await(kWaitDuration).result());
        NEARBY_LOGS(WARNING) << "Test is done. Close the socket";
        connection->channel->Close();
        connection->socket->Close();
      });

  SingleThreadExecutor server_executor;
  server_executor.Execute(
      [&handler_1, &wifi_direct_go, &upgrade_frame, &start_latch]() {
        ByteArray upgrade_path_available_frame =
            handler_1->InitializeUpgradedMediumForEndpoint(&wifi_direct_go,
                                                           /*service_id=*/"A",
                                                           /*endpoint_id=*/"1");
        EXPECT_FALSE(upgrade_path_available_frame.Empty());

        upgrade_frame = parser::FromBytes(upgrade_path_available_frame);
        start_latch.CountDown();
      });

  SingleThreadExecutor client_executor;
  // Wait till wifi_direct_go started and then connect to it
  EXPECT_TRUE(start_latch.Await(kWaitDuration).result());
  EXPECT_FALSE(mediums_2.GetWifiDirect().IsConnectedToGO());
  std::unique_ptr<BwuHandler> handler_2 =
      std::make_unique<WifiDirectBwuHandler>(mediums_2, nullptr);

  client_executor.Execute([&]() {
    auto bwu_frame =
        upgrade_frame.result().v1().bandwidth_upgrade_negotiation();

    std::unique_ptr<EndpointChannel> new_channel =
        handler_2->CreateUpgradedEndpointChannel(
            &wifi_direct_gc, /*service_id=*/"A",
            /*endpoint_id=*/"1", bwu_frame.upgrade_path_info());
    if (!FeatureFlags::GetInstance().GetFlags().enable_cancellation_flag) {
      EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
      EXPECT_EQ(new_channel->GetMedium(),
                location::nearby::proto::connections::Medium::WIFI_DIRECT);
    } else {
      accept_latch.CountDown();
      EXPECT_EQ(new_channel, nullptr);
    }
    EXPECT_TRUE(mediums_2.GetWifiDirect().IsConnectedToGO());
    handler_2->RevertResponderState(/*service_id=*/"A");
    end_latch.CountDown();
  });

  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(end_latch.Await(kWaitDuration).result());
  EXPECT_FALSE(mediums_2.GetWifiDirect().IsConnectedToGO());
}

}  // namespace connections
}  // namespace nearby
