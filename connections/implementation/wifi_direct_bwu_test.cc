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
#include "absl/time/time.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/wifi_direct_bwu_handler.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/expected.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::proto::connections::OperationResultCode;
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

    ErrorOr<std::unique_ptr<EndpointChannel>> result =
        handler_2->CreateUpgradedEndpointChannel(
            &wifi_direct_gc, /*service_id=*/"A",
            /*endpoint_id=*/"1", bwu_frame.upgrade_path_info());
    if (!FeatureFlags::GetInstance().GetFlags().enable_cancellation_flag) {
      ASSERT_TRUE(result.has_value());
      std::unique_ptr<EndpointChannel> new_channel = std::move(result.value());
      EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
      EXPECT_EQ(new_channel->GetMedium(),
                location::nearby::proto::connections::Medium::WIFI_DIRECT);
    } else {
      EXPECT_FALSE(result.has_value());
      EXPECT_TRUE(result.has_error());
      EXPECT_EQ(result.error().operation_result_code(),
                OperationResultCode::
                    CLIENT_CANCELLATION_CANCEL_WIFI_DIRECT_OUTGOING_CONNECTION);
      accept_latch.CountDown();
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
