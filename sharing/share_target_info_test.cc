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

#include "sharing/share_target_info.h"

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
#include "internal/platform/implementation/device_info.h"
#include "sharing/certificates/fake_nearby_share_certificate_manager.h"
#include "sharing/fake_nearby_connection.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_sharing_decoder_impl.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"

namespace nearby::sharing {
namespace {

constexpr absl::string_view kEndpointId = "12345";

// A test class which makes ShareTargetInfo testable since the class is
// abstract.
class TestShareTargetInfo : public ShareTargetInfo {
 public:
  TestShareTargetInfo(
      std::string endpoint_id, const ShareTarget& share_target)
      : ShareTargetInfo(std::move(endpoint_id), share_target),
        is_incoming_(share_target.is_incoming) {}

  bool IsIncoming() const override { return is_incoming_; }

  int TransferUpdateCount() { return transfer_update_count_; }

  std::optional<TransferMetadata> LastTransferMetadata() {
    return last_transfer_metadata_;
  }

  void SetOnNewConnectionResult(bool result) {
    on_new_connection_result_ = result;
  }

 protected:
  void InvokeTransferUpdateCallback(
      const TransferMetadata& metadata) override {
    ++transfer_update_count_;
    last_transfer_metadata_ = metadata;
  }

  bool OnNewConnection(NearbyConnection* connection) override {
    connection_ = connection;
    return on_new_connection_result_;
  }

 private:
  const bool is_incoming_;
  int transfer_update_count_ = 0;
  std::optional<TransferMetadata> last_transfer_metadata_;
  NearbyConnection* connection_ = nullptr;
  bool on_new_connection_result_ = true;
};

TEST(ShareTargetInfoTest, UpdateTransferMetadata) {
  ShareTarget share_target;
  TestShareTargetInfo info(std::string(kEndpointId), share_target);

  info.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build());
  info.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build());

  EXPECT_EQ(info.TransferUpdateCount(), 2);
}

TEST(ShareTargetInfoTest, UpdateTransferMetadataAfterFinalStatus) {
  ShareTarget share_target;
  TestShareTargetInfo info(std::string(kEndpointId), share_target);

  info.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kComplete)
          .build());
  info.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build());

  EXPECT_EQ(info.TransferUpdateCount(), 1);
}

TEST(ShareTargetInfoTest, SetDisconnectStatus) {
  ShareTarget share_target;
  TestShareTargetInfo info(std::string(kEndpointId), share_target);

  info.set_disconnect_status(TransferMetadata::Status::kCancelled);
  EXPECT_EQ(info.disconnect_status(), TransferMetadata::Status::kCancelled);
}

TEST(ShareTargetInfoTest, OnConnectedFails) {
  ShareTarget share_target;
  TestShareTargetInfo info(std::string(kEndpointId), share_target);
  info.SetOnNewConnectionResult(false);

  EXPECT_FALSE(info.OnConnected(absl::Now(), nullptr));
}

TEST(ShareTargetInfoTest, OnConnectedSucceeds) {
  ShareTarget share_target;
  TestShareTargetInfo info(std::string(kEndpointId), share_target);
  FakeNearbyConnection connection;
  info.SetOnNewConnectionResult(true);
  absl::Time connect_start_time = absl::Now();
  FakeContext context;

  EXPECT_TRUE(info.OnConnected(connect_start_time, &connection));
  EXPECT_EQ(info.connection_start_time(), connect_start_time);
  EXPECT_EQ(info.connection(), &connection);
}

TEST(ShareTargetInfoTest, IncomingRunPairedKeyVerificationSuccess) {
  FakeContext context;
  NearbySharingDecoderImpl nearby_sharing_decoder;
  FakeNearbyShareCertificateManager certificate_manager;
  FakeNearbyConnection connection;
  std::optional<std::vector<uint8_t>> token =
      std::vector<uint8_t>{0, 1, 2, 3, 4, 5};
  ShareTarget share_target;
  share_target.is_incoming = true;
  TestShareTargetInfo info(std::string(kEndpointId), share_target);
  info.SetOnNewConnectionResult(true);
  absl::Time connect_start_time = absl::Now();
  EXPECT_TRUE(info.OnConnected(connect_start_time, &connection));
  absl::Notification notification;
  PairedKeyVerificationRunner::PairedKeyVerificationResult verification_result;

  info.RunPairedKeyVerification(
      &context, &nearby_sharing_decoder,
      nearby::api::DeviceInfo::OsType::kWindows,
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
  connection.AppendReadableData(
      std::vector<uint8_t>(in_encryption_buffer.begin(),
                           in_encryption_buffer.end()));
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

TEST(ShareTargetInfoTest, OnDisconnect) {
  ShareTarget share_target;
  TestShareTargetInfo info(std::string(kEndpointId), share_target);
  info.set_disconnect_status(TransferMetadata::Status::kCancelled);
  EXPECT_EQ(info.disconnect_status(), TransferMetadata::Status::kCancelled);

  info.OnDisconnect();

  EXPECT_EQ(info.TransferUpdateCount(), 1);
  ASSERT_TRUE(info.LastTransferMetadata().has_value());
  EXPECT_EQ(info.LastTransferMetadata()->status(),
            TransferMetadata::Status::kCancelled);
  EXPECT_TRUE(info.LastTransferMetadata()->is_final_status());
}

}  // namespace
}  // namespace nearby::sharing
