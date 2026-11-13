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

#include "connections/implementation/incoming_payload_manager.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "connections/connection_options.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/fake_endpoint_channel.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/listeners.h"
#include "connections/payload.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/single_thread_executor.h"
#include "proto/connections_enums.pb.h"

namespace nearby::connections {
namespace {

using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::PayloadTransferFrame;
using ::location::nearby::proto::connections::Medium;
using ::location::nearby::proto::connections::OperationResultCode;
using ::location::nearby::proto::connections::PayloadStatus;

constexpr absl::string_view kEndpointA = "ABCD";
constexpr absl::string_view kEndpointB = "EFGH";
constexpr absl::string_view kServiceId = "service-id";
constexpr absl::Duration kDefaultTimeout = absl::Seconds(5);

PayloadTransferFrame MakeDataFrame(
    Payload::Id payload_id,
    PayloadTransferFrame::PayloadHeader::PayloadType type, int64_t total_size,
    int64_t offset, absl::string_view body, int32_t flags,
    absl::string_view file_name = "") {
  PayloadTransferFrame frame;
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto* header = frame.mutable_payload_header();
  header->set_id(payload_id);
  header->set_type(type);
  header->set_total_size(total_size);
  if (!file_name.empty()) {
    header->set_file_name(file_name);
  }
  auto* chunk = frame.mutable_payload_chunk();
  chunk->set_offset(offset);
  chunk->set_body(body);
  chunk->set_flags(flags);
  return frame;
}

PayloadTransferFrame MakeControlFrame(
    Payload::Id payload_id,
    PayloadTransferFrame::PayloadHeader::PayloadType type, int64_t total_size,
    int64_t offset, PayloadTransferFrame::ControlMessage::EventType event) {
  PayloadTransferFrame frame;
  frame.set_packet_type(PayloadTransferFrame::CONTROL);
  auto* header = frame.mutable_payload_header();
  header->set_id(payload_id);
  header->set_type(type);
  header->set_total_size(total_size);
  auto* control = frame.mutable_control_message();
  control->set_offset(offset);
  control->set_event(event);
  return frame;
}

class IncomingPayloadManagerTest : public ::testing::Test {
 protected:
  void SetUp() override { channel_a_ = ConnectEndpoint(kEndpointA); }

  void TearDown() override {
    shutdown_.store(true);
    incoming_payload_manager_.CancelAllPayloads();
    status_update_executor_.Shutdown();
    incoming_payload_manager_.StopTrackingAllPayloads();
    NearbyFlags::GetInstance().ResetOverridedValues();
  }

  std::shared_ptr<FakeEndpointChannel> ConnectEndpoint(
      absl::string_view endpoint_id) {
    std::string id(endpoint_id);
    auto channel = std::make_shared<FakeEndpointChannel>(
        Medium::WIFI_LAN, std::string(kServiceId));
    ConnectionResponseInfo info{
        .remote_endpoint_info = ByteArray{"remote"},
        .authentication_token = "auth",
        .raw_authentication_token = ByteArray{"auth"},
        .is_incoming_connection = true,
    };
    ConnectionOptions options{};
    ConnectionListener connection_listener{};
    client_.OnConnectionInitiated(id, info, options, connection_listener,
                                  "token");
    PayloadListener payload_listener{
        .payload_cb =
            [this](absl::string_view endpoint_id, Payload payload) {
              absl::MutexLock lock(mutex_);
              received_payloads_.emplace_back(std::string(endpoint_id),
                                              std::move(payload));
            },
        .payload_progress_cb =
            [this](absl::string_view endpoint_id,
                   const PayloadProgressInfo& info) {
              absl::MutexLock lock(mutex_);
              progress_updates_.emplace_back(std::string(endpoint_id), info);
            },
    };
    client_.LocalEndpointAcceptedConnection(id, std::move(payload_listener));
    client_.RemoteEndpointAcceptedConnection(id);
    client_.OnConnectionAccepted(id);
    channel_manager_.RegisterChannelForEndpoint(&client_, id, channel);
    return channel;
  }

  void FlushStatusUpdateThread() {
    absl::Notification latch;
    status_update_executor_.Execute("flush", [&latch]() { latch.Notify(); });
    EXPECT_TRUE(latch.WaitForNotificationWithTimeout(kDefaultTimeout));
  }

  ClientProxy client_;
  EndpointChannelManager channel_manager_;
  EndpointManager endpoint_manager_{&channel_manager_};
  SingleThreadExecutor status_update_executor_;
  std::atomic<bool> shutdown_{false};
  IncomingPayloadManager incoming_payload_manager_{
      endpoint_manager_, status_update_executor_, shutdown_};
  std::shared_ptr<FakeEndpointChannel> channel_a_;

  mutable absl::Mutex mutex_;
  std::vector<std::pair<std::string, Payload>> received_payloads_
      ABSL_GUARDED_BY(mutex_);
  std::vector<std::pair<std::string, PayloadProgressInfo>> progress_updates_
      ABSL_GUARDED_BY(mutex_);
};

TEST_F(IncomingPayloadManagerTest, ReceiveBytesPayloadSucceeds) {
  constexpr Payload::Id kPayloadId = 1001;
  constexpr absl::string_view kData = "hello world";

  PayloadTransferFrame chunk0 = MakeDataFrame(
      kPayloadId, PayloadTransferFrame::PayloadHeader::BYTES, kData.size(),
      /*offset=*/0, kData, /*flags=*/0);
  PayloadTransferFrame last_chunk = MakeDataFrame(
      kPayloadId, PayloadTransferFrame::PayloadHeader::BYTES, kData.size(),
      /*offset=*/kData.size(), /*body=*/"",
      PayloadTransferFrame::PayloadChunk::LAST_CHUNK);

  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              chunk0, Medium::WIFI_LAN);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              last_chunk, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  {
    absl::MutexLock lock(mutex_);
    ASSERT_EQ(received_payloads_.size(), 1u);
    EXPECT_EQ(received_payloads_[0].first, kEndpointA);
    EXPECT_EQ(received_payloads_[0].second.GetId(), kPayloadId);
    EXPECT_EQ(received_payloads_[0].second.AsBytes(),
              ByteArray(std::string(kData)));

    ASSERT_EQ(progress_updates_.size(), 2u);
    EXPECT_EQ(progress_updates_[0].second.payload_id, kPayloadId);
    EXPECT_EQ(progress_updates_[0].second.status,
              PayloadProgressInfo::Status::kInProgress);
    EXPECT_EQ(progress_updates_[0].second.bytes_transferred, kData.size());
    EXPECT_EQ(progress_updates_[1].second.payload_id, kPayloadId);
    EXPECT_EQ(progress_updates_[1].second.status,
              PayloadProgressInfo::Status::kSuccess);
    EXPECT_EQ(progress_updates_[1].second.bytes_transferred, kData.size());
  }

  // Completed payload should no longer be tracked.
  EXPECT_FALSE(incoming_payload_manager_.CancelPayload(kPayloadId));
}

TEST_F(IncomingPayloadManagerTest, ReceiveMultiChunkStreamPayloadSucceeds) {
  constexpr Payload::Id kPayloadId = 1002;
  constexpr absl::string_view kChunk1 = "chunk-1";
  constexpr absl::string_view kChunk2 = "chunk-2";

  PayloadTransferFrame frame1 =
      MakeDataFrame(kPayloadId, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/0, kChunk1, /*flags=*/0);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              frame1, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  InputStream* stream = nullptr;
  {
    absl::MutexLock lock(mutex_);
    ASSERT_EQ(received_payloads_.size(), 1u);
    stream = received_payloads_[0].second.AsStream();
    ASSERT_NE(stream, nullptr);
  }
  EXPECT_EQ(stream->Read(64).result().AsStringView(), kChunk1);

  PayloadTransferFrame frame2 = MakeDataFrame(
      kPayloadId, PayloadTransferFrame::PayloadHeader::STREAM,
      /*total_size=*/-1, /*offset=*/kChunk1.size(), kChunk2, /*flags=*/0);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              frame2, Medium::WIFI_LAN);
  FlushStatusUpdateThread();
  EXPECT_EQ(stream->Read(64).result().AsStringView(), kChunk2);

  PayloadTransferFrame last_frame = MakeDataFrame(
      kPayloadId, PayloadTransferFrame::PayloadHeader::STREAM,
      /*total_size=*/-1, /*offset=*/kChunk1.size() + kChunk2.size(),
      /*body=*/"", PayloadTransferFrame::PayloadChunk::LAST_CHUNK);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              last_frame, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  {
    absl::MutexLock lock(mutex_);
    ASSERT_FALSE(progress_updates_.empty());
    EXPECT_EQ(progress_updates_.back().second.status,
              PayloadProgressInfo::Status::kSuccess);
    EXPECT_EQ(progress_updates_.back().second.bytes_transferred,
              kChunk1.size() + kChunk2.size());
  }
  stream->Close();
}

#if !defined(NEARBY_CHROMIUM)
TEST_F(IncomingPayloadManagerTest, ReceiveFilePayloadUsesCustomSavePath) {
  constexpr Payload::Id kPayloadId = 1003;
  constexpr absl::string_view kFileName = "incoming_manager_test_file.txt";
  constexpr absl::string_view kContent = "file payload contents";

  std::string temp_dir = ::testing::TempDir();
  incoming_payload_manager_.SetCustomSavePath(&client_, temp_dir);

  PayloadTransferFrame chunk0 = MakeDataFrame(
      kPayloadId, PayloadTransferFrame::PayloadHeader::FILE, kContent.size(),
      /*offset=*/0, kContent, /*flags=*/0, kFileName);
  PayloadTransferFrame last_chunk = MakeDataFrame(
      kPayloadId, PayloadTransferFrame::PayloadHeader::FILE, kContent.size(),
      /*offset=*/kContent.size(), /*body=*/"",
      PayloadTransferFrame::PayloadChunk::LAST_CHUNK, kFileName);

  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              chunk0, Medium::WIFI_LAN);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              last_chunk, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  FilePath expected_path = FilePath(temp_dir).append(FilePath(kFileName));
  EXPECT_TRUE(Files::FileExists(expected_path));

  {
    absl::MutexLock lock(mutex_);
    ASSERT_EQ(received_payloads_.size(), 1u);
    ASSERT_NE(received_payloads_[0].second.AsFile(), nullptr);
    received_payloads_[0].second.AsFile()->Close();
    ASSERT_FALSE(progress_updates_.empty());
    EXPECT_EQ(progress_updates_.back().second.status,
              PayloadProgressInfo::Status::kSuccess);
  }
  Files::RemoveFile(expected_path);
}
#endif  // !defined(NEARBY_CHROMIUM)

TEST_F(IncomingPayloadManagerTest,
       PayloadIdZeroIsDeniedAndSendsErrorControlFrame) {
  PayloadTransferFrame frame = MakeDataFrame(
      /*payload_id=*/0, PayloadTransferFrame::PayloadHeader::BYTES,
      /*total_size=*/5, /*offset=*/0, "hello",
      PayloadTransferFrame::PayloadChunk::LAST_CHUNK);

  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              frame, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  {
    absl::MutexLock lock(mutex_);
    EXPECT_TRUE(received_payloads_.empty());
    EXPECT_TRUE(progress_updates_.empty());
  }

  std::vector<std::string> written_frames = channel_a_->GetWrittenFrames();
  ASSERT_EQ(written_frames.size(), 1u);
  ExceptionOr<OfflineFrame> parsed = parser::FromBytes(written_frames[0]);
  ASSERT_TRUE(parsed.ok());
  const PayloadTransferFrame& transfer =
      parsed.result().v1().payload_transfer();
  EXPECT_EQ(transfer.packet_type(), PayloadTransferFrame::CONTROL);
  EXPECT_EQ(transfer.control_message().event(),
            PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
}

TEST_F(IncomingPayloadManagerTest,
       DuplicatePayloadIdAtZeroOffsetFailsExistingPayload) {
  constexpr Payload::Id kPayloadId = 1004;

  PayloadTransferFrame chunk0 =
      MakeDataFrame(kPayloadId, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/0, "first", /*flags=*/0);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              chunk0, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  // Sending another initial chunk (offset 0) with the same payload ID must fail
  // the existing payload and send a PAYLOAD_ERROR control frame.
  PayloadTransferFrame duplicate_chunk0 =
      MakeDataFrame(kPayloadId, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/0, "duplicate", /*flags=*/0);
  incoming_payload_manager_.ProcessDataPacket(
      &client_, std::string(kEndpointA), duplicate_chunk0, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  {
    absl::MutexLock lock(mutex_);
    ASSERT_FALSE(progress_updates_.empty());
    EXPECT_EQ(progress_updates_.back().second.payload_id, kPayloadId);
    EXPECT_EQ(progress_updates_.back().second.status,
              PayloadProgressInfo::Status::kFailure);
  }
  EXPECT_FALSE(incoming_payload_manager_.CancelPayload(kPayloadId));

  std::vector<std::string> written_frames = channel_a_->GetWrittenFrames();
  ASSERT_EQ(written_frames.size(), 1u);
  ExceptionOr<OfflineFrame> parsed = parser::FromBytes(written_frames[0]);
  ASSERT_TRUE(parsed.ok());
  EXPECT_EQ(parsed.result().v1().payload_transfer().control_message().event(),
            PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
}

TEST_F(IncomingPayloadManagerTest,
       CancelPayloadReturnsFalseForUnknownAndTrueForTrackedPayload) {
  constexpr Payload::Id kPayloadId = 1005;
  EXPECT_FALSE(incoming_payload_manager_.CancelPayload(kPayloadId));

  PayloadTransferFrame chunk0 =
      MakeDataFrame(kPayloadId, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/0, "hello", /*flags=*/0);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              chunk0, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  EXPECT_TRUE(incoming_payload_manager_.CancelPayload(kPayloadId));

  // Subsequent chunk triggers local cancellation handling and sends
  // PAYLOAD_CANCELED control message to the remote endpoint.
  PayloadTransferFrame chunk1 =
      MakeDataFrame(kPayloadId, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/5, "world", /*flags=*/0);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              chunk1, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  {
    absl::MutexLock lock(mutex_);
    ASSERT_FALSE(progress_updates_.empty());
    EXPECT_EQ(progress_updates_.back().second.payload_id, kPayloadId);
    EXPECT_EQ(progress_updates_.back().second.status,
              PayloadProgressInfo::Status::kCanceled);
  }

  std::vector<std::string> written_frames = channel_a_->GetWrittenFrames();
  ASSERT_EQ(written_frames.size(), 1u);
  ExceptionOr<OfflineFrame> parsed = parser::FromBytes(written_frames[0]);
  ASSERT_TRUE(parsed.ok());
  EXPECT_EQ(parsed.result().v1().payload_transfer().control_message().event(),
            PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
}

TEST_F(IncomingPayloadManagerTest, CancelAllPayloadsMarksAllTrackedCanceled) {
  constexpr Payload::Id kPayloadId1 = 1006;
  constexpr Payload::Id kPayloadId2 = 1007;

  PayloadTransferFrame init1 =
      MakeDataFrame(kPayloadId1, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/0, "init1", /*flags=*/0);
  PayloadTransferFrame init2 =
      MakeDataFrame(kPayloadId2, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/0, "init2", /*flags=*/0);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              init1, Medium::WIFI_LAN);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              init2, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  incoming_payload_manager_.CancelAllPayloads();

  PayloadTransferFrame next1 =
      MakeDataFrame(kPayloadId1, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/5, "more1", /*flags=*/0);
  PayloadTransferFrame next2 =
      MakeDataFrame(kPayloadId2, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/5, "more2", /*flags=*/0);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              next1, Medium::WIFI_LAN);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              next2, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  EXPECT_EQ(channel_a_->GetWrittenFrames().size(), 2u);
}

TEST_F(IncomingPayloadManagerTest,
       StopTrackingAllPayloadsRemovesTrackedPayloads) {
  constexpr Payload::Id kPayloadId = 1008;

  PayloadTransferFrame init =
      MakeDataFrame(kPayloadId, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/0, "init", /*flags=*/0);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              init, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  incoming_payload_manager_.StopTrackingAllPayloads();
  EXPECT_FALSE(incoming_payload_manager_.CancelPayload(kPayloadId));
}

TEST_F(IncomingPayloadManagerTest,
       ProcessControlPacketHandlesRemoteCancelAndError) {
  constexpr Payload::Id kUnknownId = 9999;
  constexpr Payload::Id kCanceledId = 1009;
  constexpr Payload::Id kErrorId = 1010;

  PayloadTransferFrame unknown_control =
      MakeControlFrame(kUnknownId, PayloadTransferFrame::PayloadHeader::STREAM,
                       /*total_size=*/-1, /*offset=*/0,
                       PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
  EXPECT_FALSE(incoming_payload_manager_.ProcessControlPacket(
      &client_, std::string(kEndpointA), unknown_control));

  PayloadTransferFrame init_cancel =
      MakeDataFrame(kCanceledId, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/0, "hello", /*flags=*/0);
  PayloadTransferFrame init_error =
      MakeDataFrame(kErrorId, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/0, "hello", /*flags=*/0);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              init_cancel, Medium::WIFI_LAN);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              init_error, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  PayloadTransferFrame cancel_control =
      MakeControlFrame(kCanceledId, PayloadTransferFrame::PayloadHeader::STREAM,
                       /*total_size=*/-1, /*offset=*/5,
                       PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
  EXPECT_TRUE(incoming_payload_manager_.ProcessControlPacket(
      &client_, std::string(kEndpointA), cancel_control));

  PayloadTransferFrame error_control =
      MakeControlFrame(kErrorId, PayloadTransferFrame::PayloadHeader::STREAM,
                       /*total_size=*/-1, /*offset=*/5,
                       PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
  EXPECT_TRUE(incoming_payload_manager_.ProcessControlPacket(
      &client_, std::string(kEndpointA), error_control));
  FlushStatusUpdateThread();

  {
    absl::MutexLock lock(mutex_);
    ASSERT_GE(progress_updates_.size(), 2u);
    const auto& second_last =
        progress_updates_[progress_updates_.size() - 2].second;
    const auto& last = progress_updates_.back().second;
    EXPECT_EQ(second_last.payload_id, kCanceledId);
    EXPECT_EQ(second_last.status, PayloadProgressInfo::Status::kCanceled);
    EXPECT_EQ(second_last.bytes_transferred, 5);
    EXPECT_EQ(last.payload_id, kErrorId);
    EXPECT_EQ(last.status, PayloadProgressInfo::Status::kFailure);
    EXPECT_EQ(last.bytes_transferred, 5);
  }
}

TEST_F(IncomingPayloadManagerTest,
       OnEndpointDisconnectFailsOnlyDisconnectedEndpointPayloads) {
  ConnectEndpoint(kEndpointB);
  constexpr Payload::Id kPayloadA = 1011;
  constexpr Payload::Id kPayloadB = 1012;

  PayloadTransferFrame chunk0_a =
      MakeDataFrame(kPayloadA, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/0, "12345", /*flags=*/0);
  PayloadTransferFrame chunk1_a =
      MakeDataFrame(kPayloadA, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/5, "67890", /*flags=*/0);
  PayloadTransferFrame chunk0_b =
      MakeDataFrame(kPayloadB, PayloadTransferFrame::PayloadHeader::STREAM,
                    /*total_size=*/-1, /*offset=*/0, "abcde", /*flags=*/0);

  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              chunk0_a, Medium::WIFI_LAN);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointA),
                                              chunk1_a, Medium::WIFI_LAN);
  incoming_payload_manager_.ProcessDataPacket(&client_, std::string(kEndpointB),
                                              chunk0_b, Medium::WIFI_LAN);
  FlushStatusUpdateThread();

  incoming_payload_manager_.OnEndpointDisconnect(
      &client_, std::string(kEndpointA), PayloadStatus::ENDPOINT_IO_ERROR,
      OperationResultCode::CONNECTIVITY_GENERIC_WRITING_CHANNEL_IO_ERROR);

  {
    absl::MutexLock lock(mutex_);
    ASSERT_FALSE(progress_updates_.empty());
    EXPECT_EQ(progress_updates_.back().first, kEndpointA);
    EXPECT_EQ(progress_updates_.back().second.payload_id, kPayloadA);
    EXPECT_EQ(progress_updates_.back().second.status,
              PayloadProgressInfo::Status::kFailure);
    EXPECT_EQ(progress_updates_.back().second.bytes_transferred, 5);
  }

  EXPECT_FALSE(incoming_payload_manager_.CancelPayload(kPayloadA));
  EXPECT_TRUE(incoming_payload_manager_.CancelPayload(kPayloadB));
}

}  // namespace
}  // namespace nearby::connections
