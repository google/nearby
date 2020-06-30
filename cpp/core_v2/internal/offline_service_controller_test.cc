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

#include "core_v2/internal/offline_service_controller.h"

#include "core_v2/internal/offline_simulation_user.h"
#include "platform_v2/base/medium_environment.h"
#include "platform_v2/base/output_stream.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/logging.h"
#include "platform_v2/public/pipe.h"
#include "platform_v2/public/system_clock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

using ::testing::Eq;

constexpr absl::string_view kServiceId = "service-id";
constexpr absl::string_view kDeviceA = "device-a";
constexpr absl::string_view kDeviceB = "device-b";
constexpr absl::string_view kMessage = "message";
constexpr absl::Duration kProgressTimeout = absl::Milliseconds(1000);
constexpr absl::Duration kDefaultTimeout = absl::Milliseconds(1000);
constexpr absl::Duration kDisconnectTimeout = absl::Milliseconds(15000);

class OfflineServiceControllerTest : public ::testing::Test {
 protected:
  OfflineServiceControllerTest() { env_.Stop(); }

  bool SetupConnection(OfflineSimulationUser& user_a,
                       OfflineSimulationUser& user_b) {
    user_a.StartAdvertising(std::string(kServiceId), &connect_latch_);
    user_b.StartDiscovery(std::string(kServiceId), &discover_latch_);
    EXPECT_TRUE(discover_latch_.Await(kDefaultTimeout).result());
    EXPECT_EQ(user_b.GetDiscovered().service_id, kServiceId);
    EXPECT_EQ(user_b.GetDiscovered().endpoint_name, user_a.GetName());
    EXPECT_FALSE(user_b.GetDiscovered().endpoint_id.empty());
    NEARBY_LOG(INFO, "EP-B: [discovered] %s",
               user_b.GetDiscovered().endpoint_id.c_str());
    user_b.RequestConnection(&connect_latch_);
    EXPECT_TRUE(connect_latch_.Await(kDefaultTimeout).result());
    EXPECT_FALSE(user_a.GetDiscovered().endpoint_id.empty());
    NEARBY_LOG(INFO, "EP-A: [discovered] %s",
               user_a.GetDiscovered().endpoint_id.c_str());
    NEARBY_LOG(INFO, "Both users discovered their peers.");
    user_a.AcceptConnection(&accept_latch_);
    user_b.AcceptConnection(&accept_latch_);
    EXPECT_TRUE(accept_latch_.Await(kDefaultTimeout).result());
    NEARBY_LOG(INFO, "Both users reached connected state.");
    return user_a.IsConnected() && user_b.IsConnected();
  }

  CountDownLatch discover_latch_{1};
  CountDownLatch connect_latch_{2};
  CountDownLatch accept_latch_{2};
  CountDownLatch payload_latch_{1};
  MediumEnvironment& env_ = MediumEnvironment::Instance();
};

TEST_F(OfflineServiceControllerTest, CanCreateOne) {
  env_.Start();
  OfflineSimulationUser user_a(kDeviceA);
  env_.Stop();
}

TEST_F(OfflineServiceControllerTest, CanCreateMany) {
  env_.Start();
  OfflineSimulationUser user_a(kDeviceA);
  OfflineSimulationUser user_b(kDeviceB);
  env_.Stop();
}

TEST_F(OfflineServiceControllerTest, CanStartAdvertising) {
  env_.Start();
  OfflineSimulationUser user_a(kDeviceA);
  OfflineSimulationUser user_b(kDeviceB);
  EXPECT_FALSE(user_a.IsAdvertising());
  EXPECT_THAT(user_a.StartAdvertising(std::string(kServiceId), nullptr),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(user_a.IsAdvertising());
  env_.Stop();
}

TEST_F(OfflineServiceControllerTest, CanStartDiscoveryBeforeAdvertising) {
  env_.Start();
  OfflineSimulationUser user_a(kDeviceA);
  OfflineSimulationUser user_b(kDeviceB);
  EXPECT_FALSE(user_b.IsDiscovering());
  EXPECT_THAT(user_b.StartDiscovery(std::string(kServiceId), &discover_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(user_b.IsDiscovering());
  EXPECT_THAT(user_a.StartAdvertising(std::string(kServiceId), nullptr),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(discover_latch_.Await(kDefaultTimeout).result());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_F(OfflineServiceControllerTest, CanStartDiscoveryAfterAdvertising) {
  env_.Start();
  OfflineSimulationUser user_a(kDeviceA);
  OfflineSimulationUser user_b(kDeviceB);
  EXPECT_FALSE(user_b.IsDiscovering());
  EXPECT_FALSE(user_b.IsAdvertising());
  EXPECT_THAT(user_a.StartAdvertising(std::string(kServiceId), nullptr),
              Eq(Status{Status::kSuccess}));
  EXPECT_THAT(user_b.StartDiscovery(std::string(kServiceId), &discover_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(user_a.IsAdvertising());
  EXPECT_TRUE(user_b.IsDiscovering());
  EXPECT_TRUE(discover_latch_.Await(kDefaultTimeout).result());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_F(OfflineServiceControllerTest, CanStopAdvertising) {
  env_.Start();
  OfflineSimulationUser user_a(kDeviceA);
  OfflineSimulationUser user_b(kDeviceB);
  EXPECT_FALSE(user_a.IsAdvertising());
  EXPECT_THAT(user_a.StartAdvertising(std::string(kServiceId), nullptr),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(user_a.IsAdvertising());
  user_a.StopAdvertising();
  EXPECT_FALSE(user_a.IsAdvertising());
  EXPECT_THAT(user_b.StartDiscovery(std::string(kServiceId), &discover_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(user_b.IsDiscovering());
  EXPECT_FALSE(discover_latch_.Await(kDefaultTimeout).result());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_F(OfflineServiceControllerTest, CanStopDiscovery) {
  env_.Start();
  OfflineSimulationUser user_a(kDeviceA);
  OfflineSimulationUser user_b(kDeviceB);
  EXPECT_FALSE(user_b.IsDiscovering());
  EXPECT_THAT(user_b.StartDiscovery(std::string(kServiceId), &discover_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(user_b.IsDiscovering());
  user_b.StopDiscovery();
  EXPECT_FALSE(user_b.IsDiscovering());
  EXPECT_THAT(user_a.StartAdvertising(std::string(kServiceId), nullptr),
              Eq(Status{Status::kSuccess}));
  EXPECT_FALSE(discover_latch_.Await(kDefaultTimeout).result());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_F(OfflineServiceControllerTest, CanConnect) {
  env_.Start();
  OfflineSimulationUser user_a(kDeviceA);
  OfflineSimulationUser user_b(kDeviceB);
  EXPECT_THAT(user_a.StartAdvertising(std::string(kServiceId), &connect_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_THAT(user_b.StartDiscovery(std::string(kServiceId), &discover_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(discover_latch_.Await(kDefaultTimeout).result());
  EXPECT_THAT(user_b.RequestConnection(&connect_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(connect_latch_.Await(kDefaultTimeout).result());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_F(OfflineServiceControllerTest, CanAcceptConnection) {
  env_.Start();
  OfflineSimulationUser user_a(kDeviceA);
  OfflineSimulationUser user_b(kDeviceB);
  EXPECT_THAT(user_a.StartAdvertising(std::string(kServiceId), &connect_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_THAT(user_b.StartDiscovery(std::string(kServiceId), &discover_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(discover_latch_.Await(kDefaultTimeout).result());
  EXPECT_THAT(user_b.RequestConnection(&connect_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(connect_latch_.Await(kDefaultTimeout).result());
  EXPECT_THAT(user_a.AcceptConnection(&accept_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_THAT(user_b.AcceptConnection(&accept_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(accept_latch_.Await(kDefaultTimeout).result());
  EXPECT_TRUE(user_a.IsConnected());
  EXPECT_TRUE(user_b.IsConnected());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_F(OfflineServiceControllerTest, CanRejectConnection) {
  env_.Start();
  OfflineSimulationUser user_a(kDeviceA);
  OfflineSimulationUser user_b(kDeviceB);
  CountDownLatch reject_latch(1);
  EXPECT_THAT(user_a.StartAdvertising(std::string(kServiceId), &connect_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_THAT(user_b.StartDiscovery(std::string(kServiceId), &discover_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(discover_latch_.Await(kDefaultTimeout).result());
  EXPECT_THAT(user_b.RequestConnection(&connect_latch_),
              Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(connect_latch_.Await(kDefaultTimeout).result());
  user_a.ExpectRejectedConnection(reject_latch);
  EXPECT_THAT(user_b.RejectConnection(nullptr), Eq(Status{Status::kSuccess}));
  EXPECT_TRUE(reject_latch.Await(kDefaultTimeout).result());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_F(OfflineServiceControllerTest, CanSendBytePayload) {
  env_.Start();
  OfflineSimulationUser user_a(kDeviceA);
  OfflineSimulationUser user_b(kDeviceB);
  ASSERT_TRUE(SetupConnection(user_a, user_b));
  ByteArray message(std::string{kMessage});
  user_a.SendPayload(Payload(message));
  user_b.ExpectPayload(payload_latch_);
  EXPECT_TRUE(payload_latch_.Await(kDefaultTimeout).result());
  EXPECT_EQ(user_b.GetPayload().AsBytes(), message);
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_F(OfflineServiceControllerTest, CanSendStreamPayload) {
  env_.Start();
  OfflineSimulationUser user_a(kDeviceA);
  OfflineSimulationUser user_b(kDeviceB);
  ASSERT_TRUE(SetupConnection(user_a, user_b));
  ByteArray message(std::string{kMessage});
  auto pipe = std::make_shared<Pipe>();
  OutputStream& tx = pipe->GetOutputStream();
  user_a.SendPayload(Payload([pipe]() -> InputStream& {
    return pipe->GetInputStream();  // NOLINT
  }));
  user_b.ExpectPayload(payload_latch_);
  tx.Write(message);
  EXPECT_TRUE(payload_latch_.Await(kDefaultTimeout).result());
  EXPECT_NE(user_b.GetPayload().AsStream(), nullptr);
  InputStream& rx = *user_b.GetPayload().AsStream();
  ASSERT_TRUE(user_b.WaitForProgress(
      [size = message.size()](const PayloadProgressInfo& info) -> bool {
        return info.bytes_transferred >= size;
      },
      kProgressTimeout));
  EXPECT_EQ(rx.Read(Pipe::kChunkSize).result(), message);
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_F(OfflineServiceControllerTest, CanCancelStreamPayload) {
  env_.Start();
  OfflineSimulationUser user_a(kDeviceA);
  OfflineSimulationUser user_b(kDeviceB);
  ASSERT_TRUE(SetupConnection(user_a, user_b));
  ByteArray message(std::string{kMessage});
  auto pipe = std::make_shared<Pipe>();
  OutputStream& tx = pipe->GetOutputStream();
  user_a.SendPayload(Payload([pipe]() -> InputStream& {
    return pipe->GetInputStream();  // NOLINT
  }));
  user_b.ExpectPayload(payload_latch_);
  tx.Write(message);
  EXPECT_TRUE(payload_latch_.Await(kDefaultTimeout).result());
  EXPECT_NE(user_b.GetPayload().AsStream(), nullptr);
  InputStream& rx = *user_b.GetPayload().AsStream();
  ASSERT_TRUE(user_b.WaitForProgress(
      [size = message.size()](const PayloadProgressInfo& info) -> bool {
        return info.bytes_transferred >= size;
      },
      kProgressTimeout));
  EXPECT_EQ(rx.Read(Pipe::kChunkSize).result(), message);
  user_b.CancelPayload();
  int count = 0;
  while (true) {
    count++;
    if (!tx.Write(message).Ok()) break;
    SystemClock::Sleep(kDefaultTimeout);
  }
  EXPECT_TRUE(user_a.WaitForProgress(
      [](const PayloadProgressInfo& info) -> bool {
        return info.status == PayloadProgressInfo::Status::kCanceled;
      },
      kProgressTimeout));
  EXPECT_LT(count, 10);
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_F(OfflineServiceControllerTest, CanDisconnect) {
  env_.Start();
  CountDownLatch disconnect_latch(1);
  OfflineSimulationUser user_a(kDeviceA);
  OfflineSimulationUser user_b(kDeviceB);
  ASSERT_TRUE(SetupConnection(user_a, user_b));
  NEARBY_LOGS(INFO) << "Disconnecting";
  user_b.ExpectDisconnect(disconnect_latch);
  user_b.Disconnect();
  EXPECT_TRUE(disconnect_latch.Await(kDisconnectTimeout).result());
  NEARBY_LOGS(INFO) << "Disconnected";
  EXPECT_FALSE(user_b.IsConnected());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
