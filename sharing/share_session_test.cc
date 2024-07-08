// Copyright 2024 Google LLC
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

#include "sharing/share_session.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/certificates/fake_nearby_share_certificate_manager.h"
#include "sharing/fake_nearby_connection.h"
#include "sharing/fake_nearby_connections_manager.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_sharing_decoder_impl.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"

namespace nearby::sharing {
namespace {

using ::location::nearby::proto::sharing::OSType;
using ::nearby::sharing::service::proto::ConnectionResponseFrame;
using ::nearby::sharing::service::proto::Frame;
using ::nearby::sharing::service::proto::V1Frame;

constexpr absl::string_view kEndpointId = "12345";

// A test class which makes ShareSession testable since the class is abstract.
class TestShareSession : public ShareSession {
 public:
  TestShareSession(std::string endpoint_id, const ShareTarget& share_target)
      : ShareSession(fake_task_runner_, std::move(endpoint_id), share_target),
        is_incoming_(share_target.is_incoming) {}

  bool IsIncoming() const override { return is_incoming_; }

  int TransferUpdateCount() { return transfer_update_count_; }

  std::optional<TransferMetadata> LastTransferMetadata() {
    return last_transfer_metadata_;
  }

  void SetOnNewConnectionResult(bool result) {
    on_new_connection_result_ = result;
  }

  void SetAttachmentPayloadId(int64_t attachment_id, int64_t payload_id) {
    ShareSession::SetAttachmentPayloadId(attachment_id, payload_id);
  }

 protected:
  void InvokeTransferUpdateCallback(const TransferMetadata& metadata) override {
    ++transfer_update_count_;
    last_transfer_metadata_ = metadata;
  }

  bool OnNewConnection(NearbyConnection* connection) override {
    connection_ = connection;
    return on_new_connection_result_;
  }

 private:
  FakeClock fake_clock_;
  FakeTaskRunner fake_task_runner_ {&fake_clock_, 1};
  const bool is_incoming_;
  int transfer_update_count_ = 0;
  std::optional<TransferMetadata> last_transfer_metadata_;
  NearbyConnection* connection_ = nullptr;
  bool on_new_connection_result_ = true;
};

TEST(ShareSessionTest, UpdateTransferMetadata) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);

  session.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build());
  session.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build());

  EXPECT_EQ(session.TransferUpdateCount(), 2);
}

TEST(ShareSessionTest, UpdateTransferMetadataAfterFinalStatus) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);

  session.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kComplete)
          .build());
  session.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build());

  EXPECT_EQ(session.TransferUpdateCount(), 1);
}

TEST(ShareSessionTest, SetDisconnectStatus) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);

  session.set_disconnect_status(TransferMetadata::Status::kCancelled);
  EXPECT_EQ(session.disconnect_status(), TransferMetadata::Status::kCancelled);
}

TEST(ShareSessionTest, OnConnectedFails) {
  NearbySharingDecoderImpl nearby_sharing_decoder;
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  session.SetOnNewConnectionResult(false);

  EXPECT_FALSE(
      session.OnConnected(nearby_sharing_decoder, absl::Now(), nullptr));
}

TEST(ShareSessionTest, OnConnectedSucceeds) {
  NearbySharingDecoderImpl nearby_sharing_decoder;
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  session.SetOnNewConnectionResult(true);
  absl::Time connect_start_time = absl::Now();

  EXPECT_TRUE(session.OnConnected(nearby_sharing_decoder, connect_start_time,
                                  &connection));
  EXPECT_EQ(session.connection_start_time(), connect_start_time);
  EXPECT_EQ(session.connection(), &connection);
}

TEST(ShareSessionTest, IncomingRunPairedKeyVerificationSuccess) {
  FakeClock fake_clock;
  NearbySharingDecoderImpl nearby_sharing_decoder;
  FakeNearbyShareCertificateManager certificate_manager;
  FakeNearbyConnection connection;
  std::optional<std::vector<uint8_t>> token =
      std::vector<uint8_t>{0, 1, 2, 3, 4, 5};
  ShareTarget share_target;
  share_target.is_incoming = true;
  TestShareSession session(std::string(kEndpointId), share_target);
  session.SetOnNewConnectionResult(true);
  absl::Time connect_start_time = absl::Now();
  EXPECT_TRUE(session.OnConnected(nearby_sharing_decoder, connect_start_time,
                                  &connection));
  absl::Notification notification;
  PairedKeyVerificationRunner::PairedKeyVerificationResult verification_result;

  session.RunPairedKeyVerification(
      &fake_clock, OSType::WINDOWS,
      {
          .visibility = proto::DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
          .last_visibility =
              proto::DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
          .last_visibility_time = absl::Now(),
      },
      &certificate_manager, token,
      [&notification, &verification_result](
          PairedKeyVerificationRunner::PairedKeyVerificationResult result,
          location::nearby::proto::sharing::OSType) {
        verification_result = result;
        notification.Notify();
      });
  // Receive PairedKeyEncryptionFrame from remote device.
  // This will fail verification.
  nearby::sharing::service::proto::Frame in_encryption_frame;
  in_encryption_frame.set_version(nearby::sharing::service::proto::Frame::V1);
  in_encryption_frame.mutable_v1()->set_type(
      nearby::sharing::service::proto::V1Frame::PAIRED_KEY_ENCRYPTION);
  in_encryption_frame.mutable_v1()
      ->mutable_paired_key_encryption()
      ->set_signed_data("signed_data");
  std::string in_encryption_buffer = in_encryption_frame.SerializeAsString();
  connection.AppendReadableData(std::vector<uint8_t>(
      in_encryption_buffer.begin(), in_encryption_buffer.end()));
  // Receive PairedKeyResultFrame from remote device.
  nearby::sharing::service::proto::Frame in_result_frame;
  in_result_frame.set_version(nearby::sharing::service::proto::Frame::V1);
  in_result_frame.mutable_v1()->set_type(
      nearby::sharing::service::proto::V1Frame::PAIRED_KEY_RESULT);
  in_result_frame.mutable_v1()->mutable_paired_key_result()->set_status(
      nearby::sharing::service::proto::PairedKeyResultFrame::SUCCESS);
  std::string in_result_buffer = in_result_frame.SerializeAsString();
  connection.AppendReadableData(
      std::vector<uint8_t>(in_result_buffer.begin(), in_result_buffer.end()));

  // Check that PairedKeyEncryptionFrame is sent.
  std::vector<uint8_t> data = connection.GetWrittenData();
  nearby::sharing::service::proto::Frame out_encryption_frame;
  ASSERT_TRUE(out_encryption_frame.ParseFromArray(data.data(), data.size()));
  ASSERT_TRUE(out_encryption_frame.has_v1());
  ASSERT_TRUE(out_encryption_frame.v1().has_paired_key_encryption());

  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
  // Remote PairedKeyEncryptionFrame failed verification.
  EXPECT_EQ(verification_result,
            PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable);
}

TEST(ShareSessionTest, OnDisconnect) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  session.set_disconnect_status(TransferMetadata::Status::kCancelled);
  EXPECT_EQ(session.disconnect_status(), TransferMetadata::Status::kCancelled);

  session.OnDisconnect();

  EXPECT_EQ(session.TransferUpdateCount(), 1);
  ASSERT_TRUE(session.LastTransferMetadata().has_value());
  EXPECT_EQ(session.LastTransferMetadata()->status(),
            TransferMetadata::Status::kCancelled);
  EXPECT_TRUE(session.LastTransferMetadata()->is_final_status());
}

TEST(ShareSessionTest, CancelPayloads) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  session.SetAttachmentPayloadId(1, 2);
  session.SetAttachmentPayloadId(3, 4);

  FakeNearbyConnectionsManager connections_manager;
  session.CancelPayloads(connections_manager);

  EXPECT_TRUE(connections_manager.WasPayloadCanceled(2));
  EXPECT_TRUE(connections_manager.WasPayloadCanceled(4));
}

TEST(ShareSessionTest, WriteResponseFrame) {
  NearbySharingDecoderImpl nearby_sharing_decoder;
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  EXPECT_TRUE(
      session.OnConnected(nearby_sharing_decoder, absl::Now(), &connection));

  session.WriteResponseFrame(ConnectionResponseFrame::REJECT);

  std::vector<uint8_t> frame_data = connection.GetWrittenData();
  Frame frame;
  ASSERT_TRUE(frame.ParseFromArray(frame_data.data(), frame_data.size()));
  ASSERT_EQ(frame.version(), Frame::V1);
  ASSERT_EQ(frame.v1().type(), V1Frame::RESPONSE);
  EXPECT_EQ(frame.v1().connection_response().status(),
            ConnectionResponseFrame::REJECT);
}

TEST(ShareSessionTest, WriteCancelFrame) {
  NearbySharingDecoderImpl nearby_sharing_decoder;
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  EXPECT_TRUE(
      session.OnConnected(nearby_sharing_decoder, absl::Now(), &connection));

  session.WriteCancelFrame();

  std::vector<uint8_t> frame_data = connection.GetWrittenData();
  Frame frame;
  ASSERT_TRUE(frame.ParseFromArray(frame_data.data(), frame_data.size()));
  ASSERT_EQ(frame.version(), Frame::V1);
  EXPECT_EQ(frame.v1().type(), V1Frame::CANCEL);
}

}  // namespace
}  // namespace nearby::sharing
