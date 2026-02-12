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
#include <memory>
#include <queue>
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
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/certificates/fake_nearby_share_certificate_manager.h"
#include "sharing/fake_nearby_connections_manager.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connection_impl.h"
#include "sharing/nearby_connections_types.h"
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

  FakeNearbyConnectionsManager& connections_manager() {
    return connections_manager_;
  }

  FakeDeviceInfo& device_info() { return device_info_; }

  void SetNearbyConnection(NearbyConnection* connection) {
    std::vector<uint8_t> endpoint_info = {1, 2, 3, 4};
    std::vector<uint8_t> bluetooth_mac_address = {5, 6, 7, 8};
    connections_manager_.Connect(
        endpoint_info, kEndpointId, bluetooth_mac_address,
        nearby::sharing::proto::DataUsage::ONLINE_DATA_USAGE,
        TransportType::kHighQuality,
        [&](absl::string_view endpoint_id, NearbyConnection* connection,
            Status status) {});
    SetConnection(connection);
  }

  MOCK_METHOD(void, InvokeTransferUpdateCallback,
              (const TransferMetadata& metadata), (override));
  MOCK_METHOD(void, OnConnectionDisconnected, (), (override));

 private:
  FakeClock fake_clock_;
  FakeTaskRunner fake_task_runner_{&fake_clock_, 1};
  FakeNearbyConnectionsManager connections_manager_;
  FakeDeviceInfo device_info_;
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
  NearbyConnectionImpl connection(session.device_info());

  session.SetNearbyConnection(&connection);
  EXPECT_EQ(session.connection(), &connection);
}

TEST(ShareSessionTest, IncomingRunPairedKeyVerificationSuccess) {
  FakeNearbyShareCertificateManager certificate_manager;
  std::vector<uint8_t> token = {0, 1, 2, 3, 4, 5};
  ShareTarget share_target;
  share_target.is_incoming = true;
  TestShareSession session(std::string(kEndpointId), share_target);
  session.connections_manager().SetRawAuthenticationToken(kEndpointId, token);
  NearbyConnectionImpl connection(session.device_info());
  session.SetNearbyConnection(&connection);
  std::queue<std::vector<uint8_t>> frames_data;
  session.connections_manager().set_send_payload_callback(
      [&](std::unique_ptr<Payload> payload,
          std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>
              listener) {
        frames_data.push(std::move(payload->content.bytes_payload.bytes));
      });
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
  connection.WriteMessage(std::vector<uint8_t>(
      in_encryption_buffer.begin(), in_encryption_buffer.end()));
  // Receive PairedKeyResultFrame from remote device.
  nearby::sharing::service::proto::Frame in_result_frame;
  in_result_frame.set_version(nearby::sharing::service::proto::Frame::V1);
  in_result_frame.mutable_v1()->set_type(
      nearby::sharing::service::proto::V1Frame::PAIRED_KEY_RESULT);
  in_result_frame.mutable_v1()->mutable_paired_key_result()->set_status(
      nearby::sharing::service::proto::PairedKeyResultFrame::SUCCESS);
  std::string in_result_buffer = in_result_frame.SerializeAsString();
  connection.WriteMessage(
      std::vector<uint8_t>(in_result_buffer.begin(), in_result_buffer.end()));

  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(absl::Seconds(1)));

  EXPECT_EQ(frames_data.size(), 2);
  // Check that PairedKeyEncryptionFrame is sent.
  std::vector<uint8_t> data = frames_data.front();
  nearby::sharing::service::proto::Frame out_encryption_frame;
  ASSERT_TRUE(out_encryption_frame.ParseFromArray(data.data(), data.size()));
  ASSERT_TRUE(out_encryption_frame.has_v1());
  ASSERT_TRUE(out_encryption_frame.v1().has_paired_key_encryption());

  // Check that PairedKeyResultFrame is sent.
  frames_data.pop();
  std::vector<uint8_t> data2 = frames_data.front();
  nearby::sharing::service::proto::Frame out_result_frame;
  ASSERT_TRUE(out_result_frame.ParseFromArray(data2.data(), data2.size()));
  ASSERT_TRUE(out_result_frame.has_v1());
  ASSERT_TRUE(out_result_frame.v1().has_paired_key_result());
  ASSERT_EQ(out_result_frame.v1().paired_key_result().status(),
            nearby::sharing::service::proto::PairedKeyResultFrame::UNABLE);

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
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  NearbyConnectionImpl connection(session.device_info());
  session.SetNearbyConnection(&connection);
  session.SetAttachmentPayloadId(1, 2);
  session.SetAttachmentPayloadId(3, 4);

  EXPECT_TRUE(session.CancelPayloads());

  EXPECT_TRUE(session.connections_manager().WasPayloadCanceled(2));
  EXPECT_TRUE(session.connections_manager().WasPayloadCanceled(4));

  // Repeated calls to CancelPayloads returns false.
  EXPECT_FALSE(session.CancelPayloads());
}

TEST(ShareSessionTest, WriteResponseFrame) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  NearbyConnectionImpl connection(session.device_info());
  session.SetNearbyConnection(&connection);
  std::queue<std::vector<uint8_t>> frames_data;
  session.connections_manager().set_send_payload_callback(
      [&](std::unique_ptr<Payload> payload,
          std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>
              listener) {
        frames_data.push(std::move(payload->content.bytes_payload.bytes));
      });

  session.WriteResponseFrame(ConnectionResponseFrame::REJECT);

  std::vector<uint8_t> frame_data = frames_data.front();
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
  NearbyConnectionImpl connection(session.device_info());
  session.SetNearbyConnection(&connection);
  std::queue<std::vector<uint8_t>> frames_data;
  session.connections_manager().set_send_payload_callback(
      [&](std::unique_ptr<Payload> payload,
          std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>
              listener) {
        frames_data.push(std::move(payload->content.bytes_payload.bytes));
      });

  session.WriteCancelFrame();

  std::vector<uint8_t> frame_data = frames_data.front();
  Frame frame;
  ASSERT_TRUE(frame.ParseFromArray(frame_data.data(), frame_data.size()));
  ASSERT_EQ(frame.version(), Frame::V1);
  EXPECT_EQ(frame.v1().type(), V1Frame::CANCEL);
}

TEST(ShareSessionTest, ProcessKeyVerificationResultFail) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  NearbyConnectionImpl connection(session.device_info());
  session.SetNearbyConnection(&connection);
  session.SetTokenForTests("9876");

  EXPECT_FALSE(session.ProcessKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail,
      OSType::WINDOWS));
  EXPECT_EQ(session.os_type(), OSType::WINDOWS);
  EXPECT_FALSE(session.token().empty());
}

TEST(ShareSessionTest, ProcessKeyVerificationResultSelfShareSuccess) {
  ShareTarget share_target;
  share_target.for_self_share = true;
  TestShareSession session(std::string(kEndpointId), share_target);
  NearbyConnectionImpl connection(session.device_info());
  session.SetNearbyConnection(&connection);
  session.SetTokenForTests("9876");

  EXPECT_TRUE(session.ProcessKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess,
      OSType::WINDOWS));
  EXPECT_EQ(session.os_type(), OSType::WINDOWS);
  EXPECT_TRUE(session.self_share());
  EXPECT_TRUE(session.token().empty());
}

TEST(ShareSessionTest, ProcessKeyVerificationResultNotSelfShareSuccess) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  NearbyConnectionImpl connection(session.device_info());
  session.SetNearbyConnection(&connection);
  session.SetTokenForTests("9876");

  EXPECT_TRUE(session.ProcessKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess,
      OSType::WINDOWS));
  EXPECT_EQ(session.os_type(), OSType::WINDOWS);
  EXPECT_FALSE(session.self_share());
  // This means our share was done with a mutual contact.
  EXPECT_TRUE(session.token().empty());
}

TEST(ShareSessionTest, ProcessKeyVerificationResultSelfShareUnable) {
  ShareTarget share_target;
  share_target.for_self_share = true;
  TestShareSession session(std::string(kEndpointId), share_target);
  NearbyConnectionImpl connection(session.device_info());
  session.SetNearbyConnection(&connection);
  session.SetTokenForTests("9876");

  EXPECT_TRUE(session.ProcessKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable,
      OSType::WINDOWS));
  EXPECT_EQ(session.os_type(), OSType::WINDOWS);
  EXPECT_FALSE(session.self_share());
  EXPECT_FALSE(session.token().empty());
}

TEST(ShareSessionTest, ProcessKeyVerificationResultNotSelfShareUnable) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  NearbyConnectionImpl connection(session.device_info());
  session.SetNearbyConnection(&connection);
  session.SetTokenForTests("9876");

  EXPECT_TRUE(session.ProcessKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable,
      OSType::WINDOWS));
  EXPECT_EQ(session.os_type(), OSType::WINDOWS);
  EXPECT_FALSE(session.self_share());
  EXPECT_FALSE(session.token().empty());
}

TEST(ShareSessionTest, ProcessKeyVerificationResultUnknown) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  NearbyConnectionImpl connection(session.device_info());
  session.SetNearbyConnection(&connection);
  session.SetTokenForTests("9876");

  EXPECT_FALSE(session.ProcessKeyVerificationResult(
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
  NearbyConnectionImpl connection(session.device_info());
  session.SetNearbyConnection(&connection);
  EXPECT_TRUE(session.connections_manager()
                  .connection_endpoint_info(kEndpointId)
                  .has_value());
  EXPECT_CALL(session, InvokeTransferUpdateCallback(AllOf(
                           HasStatus(TransferMetadata::Status::kNotEnoughSpace),
                           IsFinalStatus())));

  session.Abort(TransferMetadata::Status::kNotEnoughSpace);

  // Verify that the connection is closed.
  EXPECT_FALSE(session.connections_manager()
                   .connection_endpoint_info(kEndpointId)
                   .has_value());
}

TEST(ShareSessionTest, Disconnect) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);
  NearbyConnectionImpl connection(session.device_info());
  session.SetNearbyConnection(&connection);
  EXPECT_TRUE(session.connections_manager()
                  .connection_endpoint_info(kEndpointId)
                  .has_value());

  session.Disconnect();

  // Verify that the connection is closed.
  EXPECT_FALSE(session.connections_manager()
                   .connection_endpoint_info(kEndpointId)
                   .has_value());
}

TEST(ShareSessionTest, DisconnectNotConnected) {
  ShareTarget share_target;
  TestShareSession session(std::string(kEndpointId), share_target);

  session.Disconnect();
}

}  // namespace
}  // namespace nearby::sharing
