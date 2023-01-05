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

#include <utility>

#include "gtest/gtest.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/wifi_hotspot_bwu_handler.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::connections::OfflineFrame;
constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
}  // namespace

class WifiHotspotTest : public testing::Test {
 protected:
  WifiHotspotTest() { env_.Start(); }
  ~WifiHotspotTest() override { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(WifiHotspotTest, CanCreateBwuHandler) {
  BwuHandler::BwuNotifications notifications{.incoming_connection_cb = {}};
  ClientProxy client;
  Mediums mediums;

  auto handler =
      std::make_unique<WifiHotspotBwuHandler>(mediums, notifications);

  handler->InitializeUpgradedMediumForEndpoint(&client, /*service_id=*/"B",
                                               /*endpoint_id=*/"2");
  handler->RevertInitiatorState();
  SUCCEED();
  handler.reset();
}

TEST_F(WifiHotspotTest, SoftAPBWUInit_STACreateEndpointChannel) {
  CountDownLatch start_latch(1);
  CountDownLatch accept_latch(1);
  CountDownLatch end_latch(1);

  BwuHandler::BwuNotifications notifications_1{
      .incoming_connection_cb =
          [&accept_latch, &end_latch](
              ClientProxy* client,
              std::unique_ptr<BwuHandler::IncomingSocketConnection>
                  mutable_connection) {
            NEARBY_LOGS(WARNING) << "Server socket connection accept call back";
            accept_latch.CountDown();
            EXPECT_TRUE(end_latch.Await(kWaitDuration).result());
          },
  };
  BwuHandler::BwuNotifications notifications_2{.incoming_connection_cb = {}};
  ClientProxy client_1, client_2;
  Mediums mediums_1, mediums_2;
  ExceptionOr<OfflineFrame> upgrade_frame;

  auto handler_1 =
      std::make_unique<WifiHotspotBwuHandler>(mediums_1, notifications_1);

  // client_1 works as Hotspot SoftAP
  SingleThreadExecutor server_executor;
  server_executor.Execute(
      [&handler_1, &client_1, &upgrade_frame, &start_latch]() {
        ByteArray upgrade_path_available_frame =
            handler_1->InitializeUpgradedMediumForEndpoint(&client_1,
                                                           /*service_id=*/"A",
                                                           /*endpoint_id=*/"1");
        EXPECT_FALSE(upgrade_path_available_frame.Empty());

        upgrade_frame = parser::FromBytes(upgrade_path_available_frame);
        start_latch.CountDown();
      });

  // client_2 works as Hotspot STA which will connect to client_1
  SingleThreadExecutor client_executor;
  // Wait till client_1 started as hotspot and then connect to it
  EXPECT_TRUE(start_latch.Await(kWaitDuration).result());
  std::unique_ptr<BwuHandler> handler_2 =
      std::make_unique<WifiHotspotBwuHandler>(mediums_2, notifications_2);

  client_executor.Execute([&handler_2, &client_2, &upgrade_frame, &accept_latch,
                           &end_latch]() {
    auto bwu_frame =
        upgrade_frame.result().v1().bandwidth_upgrade_negotiation();

    std::unique_ptr<EndpointChannel> new_channel =
        handler_2->CreateUpgradedEndpointChannel(&client_2, /*service_id=*/"A",
                                                 /*endpoint_id=*/"1",
                                                 bwu_frame.upgrade_path_info());
    EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
    EXPECT_EQ(new_channel->GetMedium(),
              location::nearby::proto::connections::Medium::WIFI_HOTSPOT);
    end_latch.CountDown();
  });

  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(end_latch.Await(kWaitDuration).result());
}

}  // namespace connections
}  // namespace nearby
