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

#include "connections/implementation/outgoing_payload_manager.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
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
#include "internal/platform/file.h"
#include "internal/platform/pipe.h"
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
constexpr absl::string_view kServiceId = "service-id";
constexpr absl::Duration kDefaultTimeout = absl::Seconds(5);

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

class OutgoingPayloadManagerTest : public ::testing::Test {
 protected:
  void SetUp() override { channel_a_ = ConnectEndpoint(kEndpointA); }

  void TearDown() override {
    shutdown_.store(true);
    outgoing_payload_manager_.CancelAllPayloadsAndWait();
    outgoing_payload_manager_.ShutdownExecutors();
    status_update_executor_.Shutdown();
    outgoing_payload_manager_.StopTrackingAllPayloads();
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
        .is_incoming_connection = false,
    };
    ConnectionOptions options{};
    ConnectionListener connection_listener{};
    client_.OnConnectionInitiated(id, info, options, connection_listener,
                                  "token");
    PayloadListener payload_listener{
        .payload_cb = [](absl::string_view, Payload) {},
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

  bool WaitForProgress(
      absl::AnyInvocable<bool(const PayloadProgressInfo&)> predicate) {
    auto cond = [this, &predicate]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
      for (const auto& [endpoint_id, info] : progress_updates_) {
        if (predicate(info)) return true;
      }
      return false;
    };
    absl::MutexLock lock(mutex_);
    return mutex_.AwaitWithTimeout(absl::Condition(&cond), kDefaultTimeout);
  }

  ClientProxy client_;
  EndpointChannelManager channel_manager_;
  EndpointManager endpoint_manager_{&channel_manager_};
  SingleThreadExecutor status_update_executor_;
  std::atomic<bool> shutdown_{false};
  OutgoingPayloadManager outgoing_payload_manager_{
      endpoint_manager_, status_update_executor_, shutdown_};
  std::shared_ptr<FakeEndpointChannel> channel_a_;

  mutable absl::Mutex mutex_;
  std::vector<std::pair<std::string, PayloadProgressInfo>> progress_updates_
      ABSL_GUARDED_BY(mutex_);
};

TEST_F(OutgoingPayloadManagerTest,
       SendBytesPayloadWritesFramesAndReportsSuccess) {
  constexpr Payload::Id kPayloadId = 2001;
  constexpr absl::string_view kData = "hello outgoing";

  outgoing_payload_manager_.SendPayload(
      &client_, {std::string(kEndpointA)},
      Payload(kPayloadId, ByteArray(std::string(kData))));

  ASSERT_TRUE(WaitForProgress([](const PayloadProgressInfo& info) {
    return info.payload_id == kPayloadId &&
           info.status == PayloadProgressInfo::Status::kSuccess;
  }));

  std::vector<std::string> written_frames = channel_a_->GetWrittenFrames();
  ASSERT_EQ(written_frames.size(), 2u);

  ExceptionOr<OfflineFrame> frame0 = parser::FromBytes(written_frames[0]);
  ASSERT_TRUE(frame0.ok());
  const PayloadTransferFrame& transfer0 =
      frame0.result().v1().payload_transfer();
  EXPECT_EQ(transfer0.packet_type(), PayloadTransferFrame::DATA);
  EXPECT_EQ(transfer0.payload_header().id(), kPayloadId);
  EXPECT_EQ(transfer0.payload_header().type(),
            PayloadTransferFrame::PayloadHeader::BYTES);
  EXPECT_EQ(transfer0.payload_header().total_size(), kData.size());
  EXPECT_EQ(transfer0.payload_chunk().offset(), 0);
  EXPECT_EQ(transfer0.payload_chunk().body(), kData);
  EXPECT_EQ(transfer0.payload_chunk().flags(), 0);

  ExceptionOr<OfflineFrame> frame1 = parser::FromBytes(written_frames[1]);
  ASSERT_TRUE(frame1.ok());
  const PayloadTransferFrame& transfer1 =
      frame1.result().v1().payload_transfer();
  EXPECT_EQ(transfer1.packet_type(), PayloadTransferFrame::DATA);
  EXPECT_EQ(transfer1.payload_chunk().offset(), kData.size());
  EXPECT_TRUE(transfer1.payload_chunk().body().empty());
  EXPECT_EQ(transfer1.payload_chunk().flags(),
            PayloadTransferFrame::PayloadChunk::LAST_CHUNK);
}

TEST_F(OutgoingPayloadManagerTest, SendFilePayloadWritesFileHeaderAndChunks) {
  constexpr absl::string_view kContent = "file-payload-body";
  std::string file_path =
      absl::StrCat(::testing::TempDir(), "/outgoing_manager_test_file.txt");
  {
    OutputFile out_file(file_path);
    ASSERT_TRUE(out_file.IsValid());
    ASSERT_TRUE(out_file.Write(kContent).Ok());
    ASSERT_TRUE(out_file.Close().Ok());
  }

  Payload file_payload("parent_dir", "custom_name.txt", InputFile(file_path));
  Payload::Id payload_id = file_payload.GetId();

  outgoing_payload_manager_.SendPayload(&client_, {std::string(kEndpointA)},
                                        std::move(file_payload));

  ASSERT_TRUE(WaitForProgress([payload_id](const PayloadProgressInfo& info) {
    return info.payload_id == payload_id &&
           info.status == PayloadProgressInfo::Status::kSuccess;
  }));

  std::vector<std::string> written_frames = channel_a_->GetWrittenFrames();
  ASSERT_EQ(written_frames.size(), 2u);

  ExceptionOr<OfflineFrame> frame0 = parser::FromBytes(written_frames[0]);
  ASSERT_TRUE(frame0.ok());
  const PayloadTransferFrame& transfer0 =
      frame0.result().v1().payload_transfer();
  EXPECT_EQ(transfer0.payload_header().id(), payload_id);
  EXPECT_EQ(transfer0.payload_header().type(),
            PayloadTransferFrame::PayloadHeader::FILE);
  EXPECT_EQ(transfer0.payload_header().file_name(), "custom_name.txt");
  EXPECT_EQ(transfer0.payload_header().parent_folder(), "parent_dir");
  EXPECT_EQ(transfer0.payload_header().total_size(), kContent.size());
  EXPECT_EQ(transfer0.payload_chunk().body(), kContent);

  Files::RemoveFile(FilePath(file_path));
}

TEST_F(OutgoingPayloadManagerTest, SendPayloadWithOffsetSkipsInitialBytes) {
  constexpr size_t kOffset = 3;
  constexpr absl::string_view kMessage = "message";

  auto [input, tx] = CreatePipe();
  tx->Write(kMessage);
  tx->Close();

  Payload stream_payload(std::move(input));
  stream_payload.SetOffset(kOffset);
  Payload::Id payload_id = stream_payload.GetId();

  outgoing_payload_manager_.SendPayload(&client_, {std::string(kEndpointA)},
                                        std::move(stream_payload));

  ASSERT_TRUE(WaitForProgress([payload_id](const PayloadProgressInfo& info) {
    return info.payload_id == payload_id &&
           info.status == PayloadProgressInfo::Status::kSuccess;
  }));

  std::vector<std::string> written_frames = channel_a_->GetWrittenFrames();
  ASSERT_GE(written_frames.size(), 2u);
  ExceptionOr<OfflineFrame> frame0 = parser::FromBytes(written_frames[0]);
  ASSERT_TRUE(frame0.ok());
  const PayloadTransferFrame& transfer0 =
      frame0.result().v1().payload_transfer();
  EXPECT_EQ(transfer0.payload_chunk().offset(), 0);
  EXPECT_EQ(transfer0.payload_chunk().body(), "sage");
}

TEST_F(OutgoingPayloadManagerTest,
       CancelPayloadStopsInFlightStreamAndSendsCancelControlFrame) {
  EXPECT_FALSE(outgoing_payload_manager_.CancelPayload(99999));

  auto [input, tx] = CreatePipe();
  tx->Write("first");

  Payload stream_payload(std::move(input));
  Payload::Id payload_id = stream_payload.GetId();

  outgoing_payload_manager_.SendPayload(&client_, {std::string(kEndpointA)},
                                        std::move(stream_payload));

  ASSERT_TRUE(WaitForProgress([payload_id](const PayloadProgressInfo& info) {
    return info.payload_id == payload_id &&
           info.status == PayloadProgressInfo::Status::kInProgress &&
           info.bytes_transferred == 5;
  }));

  // CancelPayload closes the underlying stream, unblocking DetachNextChunk()
  // and causing the send loop to report kCanceled and send PAYLOAD_CANCELED.
  EXPECT_TRUE(outgoing_payload_manager_.CancelPayload(payload_id));

  ASSERT_TRUE(WaitForProgress([payload_id](const PayloadProgressInfo& info) {
    return info.payload_id == payload_id &&
           info.status == PayloadProgressInfo::Status::kCanceled;
  }));
  outgoing_payload_manager_.ShutdownExecutors();

  std::vector<std::string> written_frames = channel_a_->GetWrittenFrames();
  ASSERT_GE(written_frames.size(), 2u);
  ExceptionOr<OfflineFrame> last_frame =
      parser::FromBytes(written_frames.back());
  ASSERT_TRUE(last_frame.ok());
  const PayloadTransferFrame& transfer =
      last_frame.result().v1().payload_transfer();
  EXPECT_EQ(transfer.packet_type(), PayloadTransferFrame::CONTROL);
  EXPECT_EQ(transfer.control_message().event(),
            PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
  tx->Close();
}

TEST_F(OutgoingPayloadManagerTest,
       ProcessControlPacketHandlesRemoteCancelAndError) {
  PayloadTransferFrame unknown_control = MakeControlFrame(
      /*payload_id=*/88888, PayloadTransferFrame::PayloadHeader::STREAM,
      /*total_size=*/-1, /*offset=*/0,
      PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
  EXPECT_FALSE(outgoing_payload_manager_.ProcessControlPacket(
      &client_, std::string(kEndpointA), unknown_control));

  // 1. Test remote PAYLOAD_CANCELED on an in-flight stream payload.
  {
    auto [input, tx] = CreatePipe();
    tx->Write("chunk1");
    Payload payload(std::move(input));
    Payload::Id payload_id = payload.GetId();

    outgoing_payload_manager_.SendPayload(&client_, {std::string(kEndpointA)},
                                          std::move(payload));
    ASSERT_TRUE(WaitForProgress([payload_id](const PayloadProgressInfo& info) {
      return info.payload_id == payload_id &&
             info.status == PayloadProgressInfo::Status::kInProgress;
    }));

    PayloadTransferFrame cancel_control = MakeControlFrame(
        payload_id, PayloadTransferFrame::PayloadHeader::STREAM,
        /*total_size=*/-1, /*offset=*/6,
        PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
    EXPECT_TRUE(outgoing_payload_manager_.ProcessControlPacket(
        &client_, std::string(kEndpointA), cancel_control));

    // Unblock DetachNextChunk() so the next SendPayloadLoop iteration observes
    // the kCanceled endpoint status.
    tx->Write("chunk2");
    ASSERT_TRUE(WaitForProgress([payload_id](const PayloadProgressInfo& info) {
      return info.payload_id == payload_id &&
             info.status == PayloadProgressInfo::Status::kCanceled;
    }));
    tx->Close();
  }

  // 2. Test remote PAYLOAD_ERROR on an in-flight stream payload.
  {
    auto [input, tx] = CreatePipe();
    tx->Write("chunk1");
    Payload payload(std::move(input));
    Payload::Id payload_id = payload.GetId();

    outgoing_payload_manager_.SendPayload(&client_, {std::string(kEndpointA)},
                                          std::move(payload));
    ASSERT_TRUE(WaitForProgress([payload_id](const PayloadProgressInfo& info) {
      return info.payload_id == payload_id &&
             info.status == PayloadProgressInfo::Status::kInProgress;
    }));

    PayloadTransferFrame error_control = MakeControlFrame(
        payload_id, PayloadTransferFrame::PayloadHeader::STREAM,
        /*total_size=*/-1, /*offset=*/6,
        PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
    EXPECT_TRUE(outgoing_payload_manager_.ProcessControlPacket(
        &client_, std::string(kEndpointA), error_control));

    tx->Write("chunk2");
    ASSERT_TRUE(WaitForProgress([payload_id](const PayloadProgressInfo& info) {
      return info.payload_id == payload_id &&
             info.status == PayloadProgressInfo::Status::kFailure;
    }));
    tx->Close();
  }
}

TEST_F(OutgoingPayloadManagerTest,
       OnEndpointDisconnectRemovesEndpointAndReportsFailure) {
  auto [input, tx] = CreatePipe();
  tx->Write("hello");
  Payload payload(std::move(input));
  Payload::Id payload_id = payload.GetId();

  outgoing_payload_manager_.SendPayload(&client_, {std::string(kEndpointA)},
                                        std::move(payload));
  ASSERT_TRUE(WaitForProgress([payload_id](const PayloadProgressInfo& info) {
    return info.payload_id == payload_id &&
           info.status == PayloadProgressInfo::Status::kInProgress &&
           info.bytes_transferred == 5;
  }));

  outgoing_payload_manager_.OnEndpointDisconnect(
      &client_, std::string(kEndpointA), PayloadStatus::ENDPOINT_IO_ERROR,
      OperationResultCode::CONNECTIVITY_GENERIC_WRITING_CHANNEL_IO_ERROR);

  ASSERT_TRUE(WaitForProgress([payload_id](const PayloadProgressInfo& info) {
    return info.payload_id == payload_id &&
           info.status == PayloadProgressInfo::Status::kFailure &&
           info.bytes_transferred == 5;
  }));
  tx->Close();
}

TEST_F(OutgoingPayloadManagerTest, ChannelWriteFailureReportsFailure) {
  channel_a_->set_write_output(Exception{Exception::kIo});

  constexpr Payload::Id kPayloadId = 2009;
  outgoing_payload_manager_.SendPayload(&client_, {std::string(kEndpointA)},
                                        Payload(kPayloadId, ByteArray("data")));

  ASSERT_TRUE(WaitForProgress([](const PayloadProgressInfo& info) {
    return info.payload_id == kPayloadId &&
           info.status == PayloadProgressInfo::Status::kFailure;
  }));
}

TEST_F(OutgoingPayloadManagerTest,
       CancelAllPayloadsAndWaitUnblocksInFlightStream) {
  auto [input, tx] = CreatePipe();
  tx->Write("hello");
  Payload payload(std::move(input));
  Payload::Id payload_id = payload.GetId();

  outgoing_payload_manager_.SendPayload(&client_, {std::string(kEndpointA)},
                                        std::move(payload));
  ASSERT_TRUE(WaitForProgress([payload_id](const PayloadProgressInfo& info) {
    return info.payload_id == payload_id &&
           info.status == PayloadProgressInfo::Status::kInProgress;
  }));

  shutdown_.store(true);
  outgoing_payload_manager_.CancelAllPayloadsAndWait();
  outgoing_payload_manager_.ShutdownExecutors();
  outgoing_payload_manager_.StopTrackingAllPayloads();

  EXPECT_FALSE(outgoing_payload_manager_.CancelPayload(payload_id));
  tx->Close();
}

}  // namespace
}  // namespace nearby::connections
