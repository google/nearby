// Copyright 2026 Google LLC
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

#include "connections/implementation/mediums/webrtc/webrtc_bwu_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/webrtc/webrtc_impl.h"
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

constexpr absl::Duration kWaitDuration = absl::Milliseconds(5000);

class WebrtcBwuTest : public ::testing::Test {
 protected:
  WebrtcBwuTest() {
    original_flags_ = FeatureFlags::GetInstance().GetFlags();
    env_.Start({.webrtc_enabled = true});
  }
  ~WebrtcBwuTest() override {
    FeatureFlags::GetMutableInstanceForTesting().SetFlags(original_flags_);
    env_.Stop();
  }

  void RunCreateEndpointChannelTest(bool enable_cancellation);

  MediumEnvironment& env_{MediumEnvironment::Instance()};
  FeatureFlags::Flags original_flags_;
};

void WebrtcBwuTest::RunCreateEndpointChannelTest(bool enable_cancellation) {
  FeatureFlags::Flags flags = original_flags_;
  flags.enable_cancellation_flag = enable_cancellation;
  FeatureFlags::GetMutableInstanceForTesting().SetFlags(flags);

  CountDownLatch start_latch(1);
  CountDownLatch accept_latch(1);
  CountDownLatch end_latch(1);

  ClientProxy client_1, client_2;
  auto webrtc_1 = std::make_unique<mediums::WebRtcImpl>();
  auto webrtc_2 = std::make_unique<mediums::WebRtcImpl>();
  ExceptionOr<OfflineFrame> upgrade_frame;

  std::unique_ptr<BwuHandler> handler_1 = std::make_unique<WebrtcBwuHandler>(
      webrtc_1.get(),
      [&](ClientProxy* client,
          std::unique_ptr<BwuHandler::IncomingSocketConnection> connection) {
        LOG(INFO) << "Handler 1 callback triggered";
        accept_latch.CountDown();
      });

  std::unique_ptr<BwuHandler> handler_2 = std::make_unique<WebrtcBwuHandler>(
      webrtc_2.get(),
      [&](ClientProxy* client,
          std::unique_ptr<BwuHandler::IncomingSocketConnection> connection) {
        LOG(INFO) << "Handler 2 callback triggered";
      });

  // Server starts advertising.
  SingleThreadExecutor server_executor;
  server_executor.Execute([&]() {
    std::string upgrade_frame_bytes =
        handler_1->InitializeUpgradedMediumForEndpoint(
            &client_1, /*upgrade_service_id=*/"A", /*endpoint_id=*/"1");
    EXPECT_FALSE(upgrade_frame_bytes.empty());

    upgrade_frame = parser::FromBytes(upgrade_frame_bytes);
    start_latch.CountDown();
  });

  // Client connects.
  EXPECT_TRUE(start_latch.Await(kWaitDuration).result());

  if (enable_cancellation) {
    client_2.AddCancellationFlag(/*endpoint_id=*/"1");
    client_2.GetCancellationFlag(/*endpoint_id=*/"1")->Cancel();
  }

  SingleThreadExecutor client_executor;
  client_executor.Execute([&]() {
    auto bwu_frame =
        upgrade_frame.result().v1().bandwidth_upgrade_negotiation();

    auto result = handler_2->CreateUpgradedEndpointChannel(
        &client_2, /*service_id=*/"A",
        /*endpoint_id=*/"1", bwu_frame.upgrade_path_info());
    if (!enable_cancellation) {
      ASSERT_TRUE(result.has_value());
      std::unique_ptr<EndpointChannel> new_channel = std::move(result.value());
      EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
      EXPECT_EQ(new_channel->GetMedium(),
                location::nearby::proto::connections::Medium::WEB_RTC);
    } else {
      EXPECT_FALSE(result.has_value());
      EXPECT_TRUE(result.has_error());
      EXPECT_EQ(result.error().operation_result_code(),
                OperationResultCode::
                    CLIENT_CANCELLATION_CANCEL_WEB_RTC_OUTGOING_CONNECTION);
      accept_latch.CountDown();
    }
    handler_1->RevertResponderState(/*service_id=*/"A");
    end_latch.CountDown();
  });

  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(end_latch.Await(kWaitDuration).result());
}

TEST_F(WebrtcBwuTest, CanCreateBwuHandler) {
  auto webrtc = std::make_unique<mediums::WebRtcImpl>();
  std::unique_ptr<BwuHandler> handler = std::make_unique<WebrtcBwuHandler>(
      webrtc.get(),
      [](ClientProxy* client,
         std::unique_ptr<BwuHandler::IncomingSocketConnection> connection) {});
  EXPECT_EQ(handler->GetUpgradeMedium(),
            location::nearby::proto::connections::Medium::WEB_RTC);
}

TEST_F(WebrtcBwuTest, CreateEndpointChannel_WithCancellation) {
  RunCreateEndpointChannelTest(true);
}

TEST_F(WebrtcBwuTest, CreateEndpointChannel_NoCancellation) {
  RunCreateEndpointChannelTest(false);
}

}  // namespace
}  // namespace connections
}  // namespace nearby
