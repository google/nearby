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

#include "connections/implementation/mediums/bluetooth_bwu_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/offline_frames.h"
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

class BluetoothBwuTest : public testing::Test {
 protected:
  BluetoothBwuTest() {
    original_flags_ = FeatureFlags::GetInstance().GetFlags();
    env_.Start();
  }
  ~BluetoothBwuTest() override {
    FeatureFlags::GetMutableInstanceForTesting().SetFlags(original_flags_);
    env_.Stop();
  }

  void RunSTACreateEndpointChannelTest(bool enable_cancellation);

  MediumEnvironment& env_{MediumEnvironment::Instance()};
  FeatureFlags::Flags original_flags_;
};

TEST_F(BluetoothBwuTest, CanCreateBwuHandler) {
  ClientProxy client;
  Mediums mediums;

  auto handler = std::make_unique<BluetoothBwuHandler>(
      &mediums.GetBluetoothRadio(), &mediums.GetBluetoothClassic(), nullptr);

  handler->InitializeUpgradedMediumForEndpoint(&client, /*service_id=*/"B",
                                               /*endpoint_id=*/"2");
  handler->RevertInitiatorState();
  SUCCEED();
  handler.reset();
}

void BluetoothBwuTest::RunSTACreateEndpointChannelTest(
    bool enable_cancellation) {
  FeatureFlags::Flags flags = original_flags_;
  flags.enable_cancellation_flag = enable_cancellation;
  FeatureFlags::GetMutableInstanceForTesting().SetFlags(flags);

  CountDownLatch start_latch(1);
  CountDownLatch accept_latch(1);
  CountDownLatch end_latch(1);

  ClientProxy client_1, client_2;
  Mediums mediums_1, mediums_2;
  ExceptionOr<OfflineFrame> upgrade_frame;

  EXPECT_TRUE(mediums_1.GetBluetoothRadio().Enable());
  EXPECT_TRUE(mediums_2.GetBluetoothRadio().Enable());

  auto handler_1 = std::make_unique<BluetoothBwuHandler>(
      &mediums_1.GetBluetoothRadio(), &mediums_1.GetBluetoothClassic(),
      [&](ClientProxy* client,
          std::unique_ptr<BwuHandler::IncomingSocketConnection>
              mutable_connection) {
        LOG(WARNING) << "Server socket connection accept call back";
        accept_latch.CountDown();
        EXPECT_TRUE(end_latch.Await(kWaitDuration).result());
      });

  // client_1 works as Bluetooth Server Device
  SingleThreadExecutor server_executor;
  server_executor.Execute([&]() {
    std::string upgrade_path_available_frame =
        handler_1->InitializeUpgradedMediumForEndpoint(&client_1,
                                                       /*service_id=*/"A",
                                                       /*endpoint_id=*/"1");
    EXPECT_FALSE(upgrade_path_available_frame.empty());

    upgrade_frame = parser::FromBytes(upgrade_path_available_frame);
    start_latch.CountDown();
  });

  // client_2 works as Bluetooth Client Device which will connect to client_1
  SingleThreadExecutor client_executor;
  // Wait till client_1 started as Bluetooth and then connect to it
  EXPECT_TRUE(start_latch.Await(kWaitDuration).result());
  std::unique_ptr<BwuHandler> handler_2 =
      std::make_unique<BluetoothBwuHandler>(
          &mediums_2.GetBluetoothRadio(), &mediums_2.GetBluetoothClassic(),
          nullptr);

  client_executor.Execute([&]() {
    auto bwu_frame =
        upgrade_frame.result().v1().bandwidth_upgrade_negotiation();

    ErrorOr<std::unique_ptr<EndpointChannel>> result =
        handler_2->CreateUpgradedEndpointChannel(&client_2, /*service_id=*/"A",
                                                 /*endpoint_id=*/"1",
                                                 bwu_frame.upgrade_path_info());
    if (!enable_cancellation) {
      ASSERT_TRUE(result.has_value());
      std::unique_ptr<EndpointChannel> new_channel = std::move(result.value());
      EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
      EXPECT_EQ(new_channel->GetMedium(),
                location::nearby::proto::connections::Medium::BLUETOOTH);
    } else {
      EXPECT_FALSE(result.has_value());
      EXPECT_TRUE(result.has_error());
      EXPECT_EQ(result.error().operation_result_code(),
                OperationResultCode::
                    CLIENT_CANCELLATION_CANCEL_BT_OUTGOING_CONNECTION);
      accept_latch.CountDown();
    }
    EXPECT_TRUE(mediums_2.GetBluetoothClassic().GetAddress().IsSet());
    handler_2->RevertResponderState(/*service_id=*/"A");
    end_latch.CountDown();
  });

  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(end_latch.Await(kWaitDuration).result());
}

TEST_F(BluetoothBwuTest,
       SoftAPBWUInit_STACreateEndpointChannel_WithCancellation) {
  RunSTACreateEndpointChannelTest(true);
}

TEST_F(BluetoothBwuTest,
       SoftAPBWUInit_STACreateEndpointChannel_NoCancellation) {
  RunSTACreateEndpointChannelTest(false);
}

}  // namespace connections
}  // namespace nearby
