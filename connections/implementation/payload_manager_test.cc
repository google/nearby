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

#include "connections/implementation/payload_manager.h"

#include <cstddef>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/implementation/analytics/packet_meta_data.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/simulation_user.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/pipe.h"

namespace nearby {
namespace connections {
namespace {
using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::PayloadTransferFrame;
using ::location::nearby::proto::connections::Medium;
using ::nearby::analytics::PacketMetaData;

constexpr size_t kChunkSize = 64 * 1024;
constexpr absl::string_view kServiceId = "service-id";
constexpr absl::string_view kDeviceA = "device-a";
constexpr absl::string_view kDeviceB = "device-b";
constexpr absl::string_view kMessage = "message";
constexpr absl::Duration kProgressTimeout = absl::Milliseconds(1000);
constexpr absl::Duration kDefaultTimeout = absl::Milliseconds(1000);

constexpr BooleanMediumSelector kTestCases[] = {
    BooleanMediumSelector{
        .ble = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
    },
    BooleanMediumSelector{
        .wifi_lan = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
        .ble = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
        .wifi_lan = true,
    },
    BooleanMediumSelector{
        .ble = true,
        .wifi_lan = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
        .ble = true,
        .wifi_lan = true,
    },
};

class PayloadSimulationUser : public SimulationUser {
 public:
  explicit PayloadSimulationUser(
      absl::string_view name,
      BooleanMediumSelector allowed = BooleanMediumSelector())
      : SimulationUser(std::string(name), allowed) {}
  ~PayloadSimulationUser() override {
    LOG(INFO) << "PayloadSimulationUser: [down] name=" << info_.data();
    // SystemClock::Sleep(kDefaultTimeout);
  }

  Payload& GetPayload() { return payload_; }
  void SendPayload(Payload payload) {
    sender_payload_id_ = payload.GetId();
    pm_.SendPayload(&client_, {discovered_.endpoint_id}, std::move(payload));
  }

  void ReceivePayload(Payload payload, std::string from_payload_id) {
    PayloadTransferFrame::PayloadHeader header;
    header.set_id(payload.GetId());
    header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
    header.set_total_size(payload.AsBytes().size());
    header.set_file_name("test_file.txt");
    header.set_parent_folder("");
    PayloadTransferFrame::PayloadChunk chunk;
    chunk.set_body(payload.AsBytes().data());
    chunk.set_offset(payload.GetOffset());
    chunk.set_flags(1);

    OfflineFrame offline_frame;

    ByteArray bytes = parser::ForDataPayloadTransfer(header, chunk);
    offline_frame.ParseFromString(std::string(bytes));

    PacketMetaData packet_meta_data;

    pm_.OnIncomingFrame(offline_frame, from_payload_id, &client_,
                        Medium::WIFI_HOTSPOT, packet_meta_data);
  }

  Status CancelPayload() {
    if (sender_payload_id_) {
      return pm_.CancelPayload(&client_, sender_payload_id_);
    } else {
      return pm_.CancelPayload(&client_, payload_.GetId());
    }
  }

  bool IsConnected() const {
    return client_.IsConnectedToEndpoint(discovered_.endpoint_id);
  }

 protected:
  Payload::Id sender_payload_id_ = 0;
};

class PayloadManagerTest
    : public ::testing::TestWithParam<BooleanMediumSelector> {
 protected:
  bool SetupConnection(PayloadSimulationUser& user_a,
                       PayloadSimulationUser& user_b) {
    user_a.StartAdvertising(std::string(kServiceId), &connection_latch_);
    user_b.StartDiscovery(std::string(kServiceId), &discovery_latch_);
    EXPECT_TRUE(discovery_latch_.Await(kDefaultTimeout).result());
    EXPECT_EQ(user_b.GetDiscovered().service_id, kServiceId);
    EXPECT_EQ(user_b.GetDiscovered().endpoint_info, user_a.GetInfo());
    EXPECT_FALSE(user_b.GetDiscovered().endpoint_id.empty());
    LOG(INFO) << "EP-B: [discovered] " << user_b.GetDiscovered().endpoint_id;
    user_b.RequestConnection(&connection_latch_);
    EXPECT_TRUE(connection_latch_.Await(kDefaultTimeout).result());
    EXPECT_FALSE(user_a.GetDiscovered().endpoint_id.empty());
    LOG(INFO) << "EP-A: [discovered] " << user_a.GetDiscovered().endpoint_id;
    LOG(INFO) << "Both users discovered their peers.";
    user_a.AcceptConnection(&accept_latch_);
    user_b.AcceptConnection(&accept_latch_);
    EXPECT_TRUE(accept_latch_.Await(kDefaultTimeout).result());
    LOG(INFO) << "Both users reached connected state.";
    return user_a.IsConnected() && user_b.IsConnected();
  }

  CountDownLatch discovery_latch_{1};
  CountDownLatch connection_latch_{2};
  CountDownLatch accept_latch_{2};
  CountDownLatch payload_latch_{1};
  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(PayloadManagerTest, CanCreateOne) {
  env_.Start();
  PayloadSimulationUser user_a(kDeviceA, GetParam());
  env_.Stop();
}

TEST_P(PayloadManagerTest, CanCreateMultiple) {
  env_.Start();
  PayloadSimulationUser user_a(kDeviceA, GetParam());
  PayloadSimulationUser user_b(kDeviceB, GetParam());
  env_.Stop();
}

TEST_P(PayloadManagerTest, CanSendBytePayload) {
  env_.Start();
  PayloadSimulationUser user_a(kDeviceA, GetParam());
  PayloadSimulationUser user_b(kDeviceB, GetParam());
  ASSERT_TRUE(SetupConnection(user_a, user_b));

  user_a.ExpectPayload(payload_latch_);
  user_b.SendPayload(Payload(ByteArray{std::string(kMessage)}));
  EXPECT_TRUE(payload_latch_.Await(kDefaultTimeout).result());
  EXPECT_EQ(user_a.GetPayload().AsBytes(), ByteArray(std::string(kMessage)));
  LOG(INFO) << "Test completed.";

  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_P(PayloadManagerTest, PayloadId0IsError) {
  env_.Start();
  PayloadSimulationUser user_a(kDeviceA, GetParam());
  PayloadSimulationUser user_b(kDeviceB, GetParam());
  ASSERT_TRUE(SetupConnection(user_a, user_b));

  user_a.ExpectPayload(payload_latch_);
  user_b.SendPayload(Payload(0, ByteArray{std::string(kMessage)}));
  EXPECT_FALSE(payload_latch_.Await(kDefaultTimeout).result());

  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_P(PayloadManagerTest, CanSendStreamPayload) {
  env_.Start();
  PayloadSimulationUser user_a(kDeviceA, GetParam());
  PayloadSimulationUser user_b(kDeviceB, GetParam());
  ASSERT_TRUE(SetupConnection(user_a, user_b));

  auto [input, tx] = CreatePipe();
  user_a.ExpectPayload(payload_latch_);
  // The first write to the output stream will send the first PAYLOAD_TRANSFER
  // packet with payload info and message data.
  tx->Write(kMessage);

  user_b.SendPayload(Payload(std::move(input)));
  ASSERT_TRUE(payload_latch_.Await(kDefaultTimeout).result());
  ASSERT_NE(user_a.GetPayload().AsStream(), nullptr);
  InputStream& rx = *user_a.GetPayload().AsStream();
  LOG(INFO) << "Stream extracted.";

  EXPECT_TRUE(user_a.WaitForProgress(
      [](const PayloadProgressInfo& info) {
        return info.bytes_transferred >= kMessage.size();
      },
      kProgressTimeout));
  ByteArray result = rx.Read(kChunkSize).result();
  EXPECT_EQ(result.AsStringView(), kMessage);
  LOG(INFO) << "Packet 1 handled.";

  tx->Write(kMessage);
  EXPECT_TRUE(user_a.WaitForProgress(
      [](const PayloadProgressInfo& info) {
        return info.bytes_transferred >= 2 * kMessage.size();
      },
      kProgressTimeout));
  ByteArray result2 = rx.Read(kChunkSize).result();
  EXPECT_EQ(result2.AsStringView(), kMessage);
  LOG(INFO) << "Packet 2 handled.";

  rx.Close();
  tx->Close();
  LOG(INFO) << "Test completed.";
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_P(PayloadManagerTest, CanCancelPayloadOnReceiverSide) {
  env_.Start();
  PayloadSimulationUser user_a(kDeviceA, GetParam());
  PayloadSimulationUser user_b(kDeviceB, GetParam());
  ASSERT_TRUE(SetupConnection(user_a, user_b));
  auto [input, tx] = CreatePipe();
  user_a.ExpectPayload(payload_latch_);
  tx->Write(kMessage);

  user_b.SendPayload(Payload(std::move(input)));
  ASSERT_TRUE(payload_latch_.Await(kDefaultTimeout).result());
  ASSERT_NE(user_a.GetPayload().AsStream(), nullptr);
  InputStream& rx = *user_a.GetPayload().AsStream();
  LOG(INFO) << "Stream extracted.";

  EXPECT_TRUE(user_a.WaitForProgress(
      [](const PayloadProgressInfo& info) {
        return info.bytes_transferred >= kMessage.size();
      },
      kProgressTimeout));
  ByteArray result = rx.Read(kChunkSize).result();
  EXPECT_EQ(result.AsStringView(), kMessage);
  LOG(INFO) << "Packet 1 handled.";

  EXPECT_EQ(user_a.CancelPayload(), Status{Status::kSuccess});
  LOG(INFO) << "Stream canceled on receiver side.";

  // Sender will only handle cancel event if it is sending.
  // Once cancel is handled, write will fail.
  int count = 0;
  while (true) {
    if (!tx->Write(kMessage).Ok()) break;
    SystemClock::Sleep(kDefaultTimeout);
    count++;
  }
  ASSERT_LE(count, 10);

  EXPECT_TRUE(user_a.WaitForProgress(
      [status = PayloadProgressInfo::Status::kCanceled](
          const PayloadProgressInfo& info) { return info.status == status; },
      kProgressTimeout));
  LOG(INFO) << "Stream cancelation received.";

  tx->Close();
  rx.Close();

  LOG(INFO) << "Test completed.";
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_P(PayloadManagerTest, CanCancelPayloadOnSenderSide) {
  env_.Start();
  PayloadSimulationUser user_a(kDeviceA, GetParam());
  PayloadSimulationUser user_b(kDeviceB, GetParam());
  ASSERT_TRUE(SetupConnection(user_a, user_b));
  auto [input, tx] = CreatePipe();
  user_a.ExpectPayload(payload_latch_);
  tx->Write(kMessage);

  user_b.SendPayload(Payload(std::move(input)));
  ASSERT_TRUE(payload_latch_.Await(kDefaultTimeout).result());
  ASSERT_NE(user_a.GetPayload().AsStream(), nullptr);
  InputStream& rx = *user_a.GetPayload().AsStream();
  LOG(INFO) << "Stream extracted.";

  EXPECT_TRUE(user_a.WaitForProgress(
      [](const PayloadProgressInfo& info) {
        return info.bytes_transferred >= kMessage.size();
      },
      kProgressTimeout));
  ByteArray result = rx.Read(kChunkSize).result();
  EXPECT_EQ(result.AsStringView(), kMessage);
  LOG(INFO) << "Packet 1 handled.";

  EXPECT_EQ(user_b.CancelPayload(), Status{Status::kSuccess});
  LOG(INFO) << "Stream canceled on sender side.";

  // Sender will only handle cancel event if it is sending.
  // Once cancel is handled, write will fail.
  int count = 0;
  while (true) {
    if (!tx->Write(kMessage).Ok()) break;
    SystemClock::Sleep(kDefaultTimeout);
    count++;
  }
  ASSERT_LE(count, 10);

  EXPECT_TRUE(user_a.WaitForProgress(
      [status = PayloadProgressInfo::Status::kCanceled](
          const PayloadProgressInfo& info) { return info.status == status; },
      kProgressTimeout));
  LOG(INFO) << "Stream cancelation received.";

  tx->Close();
  rx.Close();

  LOG(INFO) << "Test completed.";
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_P(PayloadManagerTest, SendPayloadWithSkip_StreamPayload) {
  constexpr size_t kOffset = 3;
  env_.Start();
  PayloadSimulationUser user_a(kDeviceA, GetParam());
  PayloadSimulationUser user_b(kDeviceB, GetParam());
  ASSERT_TRUE(SetupConnection(user_a, user_b));
  auto [input, tx] = CreatePipe();
  user_a.ExpectPayload(payload_latch_);
  // The first write to the output stream will send the first PAYLOAD_TRANSFER
  // packet with payload info and message data.
  tx->Write(kMessage);

  Payload payload(std::move(input));
  payload.SetOffset(kOffset);
  user_b.SendPayload(std::move(payload));
  ASSERT_TRUE(payload_latch_.Await(kDefaultTimeout).result());
  ASSERT_NE(user_a.GetPayload().AsStream(), nullptr);
  InputStream& rx = *user_a.GetPayload().AsStream();
  LOG(INFO) << "Stream extracted.";

  EXPECT_TRUE(user_a.WaitForProgress(
      [](const PayloadProgressInfo& info) {
        return info.bytes_transferred >= kMessage.size() - kOffset;
      },
      kProgressTimeout));
  ByteArray result = rx.Read(kChunkSize).result();
  EXPECT_EQ(result, ByteArray("sage"));
  LOG(INFO) << "Packet 1 handled.";

  tx->Write(kMessage);
  EXPECT_TRUE(user_a.WaitForProgress(
      [](const PayloadProgressInfo& info) {
        return info.bytes_transferred >= 2 * kMessage.size() - kOffset;
      },
      kProgressTimeout));
  ByteArray result2 = rx.Read(kChunkSize).result();
  EXPECT_EQ(result2.AsStringView(), kMessage);
  LOG(INFO) << "Packet 2 handled.";

  rx.Close();
  tx->Close();
  LOG(INFO) << "Test completed.";
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_P(PayloadManagerTest, OfflineFrame_BeforeConnected_ShouldDrop) {
  env_.Start();
  PayloadSimulationUser user(kDeviceB, GetParam());
  auto [input, tx] = CreatePipe();
  tx->Write(kMessage);
  Payload payload(std::move(input));
  user.ReceivePayload(std::move(payload), "1234");
  ASSERT_EQ(user.GetPayload().AsStream(), nullptr);
  user.Stop();
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedPayloadManagerTest, PayloadManagerTest,
                         ::testing::ValuesIn(kTestCases));

}  // namespace
}  // namespace connections
}  // namespace nearby
