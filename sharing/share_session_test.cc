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
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/analytics/mock_event_logger.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/certificates/fake_nearby_share_certificate_manager.h"
#include "sharing/fake_nearby_connection.h"
#include "sharing/fake_nearby_connections_manager.h"
#include "sharing/nearby_connection.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"
#include "sharing/transfer_metadata_matchers.h"

namespace nearby::sharing {
namespace {

using ::location::nearby::proto::sharing::OSType;
using ::nearby::sharing::service::proto::ConnectionResponseFrame;
using ::nearby::sharing::service::proto::Frame;
using ::nearby::sharing::service::proto::V1Frame;
using ::testing::AllOf;

constexpr absl::string_view kEndpointId = "12345";

// A test class which makes ShareSession testable since the class is abstract.
class TestShareSession : public ShareSession {
 public:
  TestShareSession(std::string endpoint_id, const ShareTarget& share_target)
      : ShareSession(&fake_clock_, fake_task_runner_, &connections_manager_,
                     analytics_recorder_, std::move(endpoint_id), share_target),
        is_incoming_(share_target.is_incoming) {}

  bool IsIncoming() const override { return is_incoming_; }

  void SetAttachmentPayloadId(int64_t attachment_id, int64_t payload_id) {
    ShareSession::SetAttachmentPayloadId(attachment_id, payload_id);
  }

  bool HandleKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult result,
      OSType share_target_os_type) {
    return ShareSession::HandleKeyVerificationResult(result,
                                                     share_target_os_type);
  }

  FakeNearbyConnectionsManager& connections_manager() {
    return connections_manager_;
  }

  void SetNearbyConnection(NearbyConnection* connection) {
    SetConnection(connection);
  }

  MOCK_METHOD(void, InvokeTransferUpdateCallback,
              (const TransferMetadata& metadata), (override));
  MOCK_METHOD(void, OnConnectionDisconnected, (), (override));

 private:
  FakeClock fake_clock_;
  FakeTaskRunner fake_task_runner_{&fake_clock_, 1};
  FakeNearbyConnectionsManager connections_manager_;
  nearby::analytics::MockEventLogger mock_event_logger_;
  analytics::AnalyticsRecorder analytics_recorder_{/*vendor_id=*/0,
                                                   &mock_event_logger_};
  const bool is_incoming_;
};

TEST(ShareSessionTest, UpdateTransferMetadata) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  EXPECT_CALL(session, InvokeTransferUpdateCallback(
                           HasStatus(TransferMetadata::Status::kInProgress)))
      .Times(2);

  session.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build());
  session.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build());
}

TEST(ShareSessionTest, UpdateTransferMetadataAfterFinalStatus) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  EXPECT_CALL(session, InvokeTransferUpdateCallback(
                           HasStatus(TransferMetadata::Status::kComplete)));

  session.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kComplete)
          .build());
  session.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build());
}

TEST(ShareSessionTest, SetDisconnectStatus) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);

  session.set_disconnect_status(TransferMetadata::Status::kCancelled);
  EXPECT_EQ(session.disconnect_status(), TransferMetadata::Status::kCancelled);
}

TEST(ShareSessionTest, OnConnectedSucceeds) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;

  session.SetNearbyConnection(&connection);
  EXPECT_EQ(session.connection(), &connection);
}

TEST(ShareSessionTest, IncomingRunPairedKeyVerificationSuccess) {
  FakeNearbyShareCertificateManager certificate_manager;
  FakeNearbyConnection connection;
  std::vector<uint8_t> token = {0, 1, 2, 3, 4, 5};
  ShareTarget share_target;
  share_target.is_incoming = true;
  TestShareSession session(std::string(kEndpointId), share_target);
  session.connections_manager().SetRawAuthenticationToken(kEndpointId, token);
  session.SetNearbyConnection(&connection);
  absl::Notification notification;
  PairedKeyVerificationRunner::PairedKeyVerificationResult verification_result;

  session.RunPairedKeyVerification(
      OSType::WINDOWS,
      {
          .visibility = proto::DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
          .last_visibility =
              proto::DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
          .last_visibility_time = absl::Now(),
      },
      &certificate_manager,
      [&notification, &verification_result](
          PairedKeyVerificationRunner::PairedKeyVerificationResult result,
          location::nearby::proto::sharing::OSType) {
        verification_result = result;
        notification.Notify();
      });
  // 8929 is the hash of the token.
  EXPECT_EQ(session.token(), "8929");
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
  EXPECT_CALL(session, InvokeTransferUpdateCallback(AllOf(
                           HasStatus(TransferMetadata::Status::kCancelled),
                           IsFinalStatus())));
  EXPECT_CALL(session, OnConnectionDisconnected());

  session.OnDisconnect();
}

TEST(ShareSessionTest, CancelPayloads) {
  FakeNearbyConnection connection;
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  session.SetNearbyConnection(&connection);
  session.SetAttachmentPayloadId(1, 2);
  session.SetAttachmentPayloadId(3, 4);

  session.CancelPayloads();

  EXPECT_TRUE(session.connections_manager().WasPayloadCanceled(2));
  EXPECT_TRUE(session.connections_manager().WasPayloadCanceled(4));
}

TEST(ShareSessionTest, WriteResponseFrame) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  session.SetNearbyConnection(&connection);

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
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  session.SetNearbyConnection(&connection);

  session.WriteCancelFrame();

  std::vector<uint8_t> frame_data = connection.GetWrittenData();
  Frame frame;
  ASSERT_TRUE(frame.ParseFromArray(frame_data.data(), frame_data.size()));
  ASSERT_EQ(frame.version(), Frame::V1);
  EXPECT_EQ(frame.v1().type(), V1Frame::CANCEL);
}

TEST(ShareSessionTest, HandleKeyVerificationResultFail) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  session.SetNearbyConnection(&connection);
  session.SetTokenForTests("9876");

  EXPECT_FALSE(session.HandleKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail,
      OSType::WINDOWS));
  EXPECT_EQ(session.os_type(), OSType::WINDOWS);
  EXPECT_FALSE(session.token().empty());
}

TEST(ShareSessionTest, HandleKeyVerificationResultSelfShareSuccess) {
  ShareTarget share_target;
  share_target.for_self_share = true;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  session.SetNearbyConnection(&connection);
  session.SetTokenForTests("9876");

  EXPECT_TRUE(session.HandleKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess,
      OSType::WINDOWS));
  EXPECT_EQ(session.os_type(), OSType::WINDOWS);
  EXPECT_TRUE(session.self_share());
  EXPECT_TRUE(session.token().empty());
}

TEST(ShareSessionTest, HandleKeyVerificationResultNotSelfShareSuccess) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  session.SetNearbyConnection(&connection);
  session.SetTokenForTests("9876");

  EXPECT_TRUE(session.HandleKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess,
      OSType::WINDOWS));
  EXPECT_EQ(session.os_type(), OSType::WINDOWS);
  EXPECT_FALSE(session.self_share());
  // This means our share was done with a mutual contact.
  EXPECT_TRUE(session.token().empty());
}

TEST(ShareSessionTest, HandleKeyVerificationResultSelfShareUnable) {
  ShareTarget share_target;
  share_target.for_self_share = true;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  session.SetNearbyConnection(&connection);
  session.SetTokenForTests("9876");

  EXPECT_TRUE(session.HandleKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable,
      OSType::WINDOWS));
  EXPECT_EQ(session.os_type(), OSType::WINDOWS);
  EXPECT_FALSE(session.self_share());
  EXPECT_FALSE(session.token().empty());
}

TEST(ShareSessionTest, HandleKeyVerificationResultNotSelfShareUnable) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  session.SetNearbyConnection(&connection);
  session.SetTokenForTests("9876");

  EXPECT_TRUE(session.HandleKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable,
      OSType::WINDOWS));
  EXPECT_EQ(session.os_type(), OSType::WINDOWS);
  EXPECT_FALSE(session.self_share());
  EXPECT_FALSE(session.token().empty());
}

TEST(ShareSessionTest, HandleKeyVerificationResultUnknown) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  session.SetNearbyConnection(&connection);
  session.SetTokenForTests("9876");

  EXPECT_FALSE(session.HandleKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnknown,
      OSType::WINDOWS));
  EXPECT_EQ(session.os_type(), OSType::WINDOWS);
  EXPECT_FALSE(session.token().empty());
}

TEST(ShareSessionTest, AbortNotConnected) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  EXPECT_CALL(session, InvokeTransferUpdateCallback(AllOf(
                           HasStatus(TransferMetadata::Status::kNotEnoughSpace),
                           IsFinalStatus())));

  session.Abort(TransferMetadata::Status::kNotEnoughSpace);
}

TEST(ShareSessionTest, AbortConnected) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  bool disconnected = false;
  connection.SetDisconnectionListener(
      [&disconnected]() { disconnected = true; });
  session.SetNearbyConnection(&connection);
  EXPECT_CALL(session, InvokeTransferUpdateCallback(AllOf(
                           HasStatus(TransferMetadata::Status::kNotEnoughSpace),
                           IsFinalStatus())));

  session.Abort(TransferMetadata::Status::kNotEnoughSpace);

  EXPECT_TRUE(disconnected);
}

TEST(ShareSessionTest, Disconnect) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  bool disconnected = false;
  connection.SetDisconnectionListener(
      [&disconnected]() { disconnected = true; });
  session.SetNearbyConnection(&connection);

  session.Disconnect();

  EXPECT_TRUE(disconnected);
}

TEST(ShareSessionTest, DisconnectNotConnected) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);

  session.Disconnect();
}

}  // namespace
}  // namespace nearby::sharing
