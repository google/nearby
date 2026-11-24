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

#include "sharing/payload_tracker.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/test/fake_clock.h"
#include "internal/test/test_single_thread_executor.h"
#include "sharing/attachment_container.h"
#include "sharing/file_attachment.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"

namespace nearby::sharing {
namespace {

constexpr int64_t kShareTargetId = 123456789L;
constexpr int64_t kFileId = 1;
constexpr int64_t kFileSize = 100 * 1024;  // 100KB
constexpr absl::string_view kFileName = "test.jpg";
constexpr absl::string_view kMimeType = "image/jpg";

class PayloadTrackerTest : public ::testing::Test {
 public:
  void SetUp() override {
    container_ =
        AttachmentContainer::Builder()
            .AddFileAttachment(FileAttachment(
                kFileId, kFileSize, std::string(kFileName),
                std::string(kMimeType), service::proto::FileMetadata::IMAGE))
            .Build();
    attachment_payload_map_.clear();
    attachment_payload_map_.emplace(container_->GetFileAttachments()[0].id(),
                                 kFileId);
    auto payload_updates_queue =
        std::make_unique<PayloadTracker::PayloadUpdateQueue>(&task_runner_);
    payload_tracker_ = std::make_unique<PayloadTracker>(
        &fake_clock_, kShareTargetId, *container_, attachment_payload_map_,
        std::move(payload_updates_queue));
  }

  void FastForward(absl::Duration duration) {
    fake_clock_.FastForward(duration);
  }

  std::optional<TransferMetadata> PayloadUpdate(int bytes_transferred) {
    auto transfer_update = std::make_unique<PayloadTransferUpdate>(
        /*payload_id=*/kFileId, PayloadStatus::kInProgress,
        /*total_bytes=*/kFileSize, /*bytes_transferred=*/bytes_transferred);
    std::optional<TransferMetadataBuilder> metadata_builder =
        payload_tracker_->ProcessPayloadUpdate(std::move(transfer_update));
    if (!metadata_builder.has_value()) {
      return std::nullopt;
    }
    return metadata_builder->build();
  }

 private:
  FakeClock fake_clock_;
  TestSingleThreadExecutor task_runner_{&fake_clock_};
  std::unique_ptr<PayloadTracker> payload_tracker_ = nullptr;
  std::unique_ptr<AttachmentContainer> container_;
  absl::flat_hash_map<int64_t, int64_t> attachment_payload_map_;
};

TEST_F(PayloadTrackerTest, StatusUpdateWithoutTimeUpdate) {
  std::optional<TransferMetadata> metadata = PayloadUpdate(1024);
  EXPECT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->progress(), 1.0);
  metadata = PayloadUpdate(2048);
  EXPECT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->progress(), 2.0);
}

TEST_F(PayloadTrackerTest, StatusUpdateWithTimeUpdate) {
  std::optional<TransferMetadata> metadata = PayloadUpdate(1024);
  EXPECT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->progress(), 1.0);
  FastForward(absl::Milliseconds(100));
  metadata = PayloadUpdate(2048);
  EXPECT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->progress(), 2.0);
  FastForward(absl::Milliseconds(100));
  metadata = PayloadUpdate(3072);
  EXPECT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->progress(), 3.0);
}

TEST(PayloadTrackerEdgeCasesTest, ZeroSpeedElapsedWindowClampsEtaToZero) {
  FakeClock fake_clock;
  TestSingleThreadExecutor task_runner(&fake_clock);
  constexpr int64_t kSecondFileId = 2;
  std::unique_ptr<AttachmentContainer> container =
      AttachmentContainer::Builder()
          .AddFileAttachment(FileAttachment(
              kFileId, kFileSize, std::string(kFileName),
              std::string(kMimeType), service::proto::FileMetadata::IMAGE))
          .AddFileAttachment(FileAttachment(
              kSecondFileId, kFileSize, "second.jpg", std::string(kMimeType),
              service::proto::FileMetadata::IMAGE))
          .Build();
  absl::flat_hash_map<int64_t, int64_t> attachment_payload_map = {
      {kFileId, kFileId},
      {kSecondFileId, kSecondFileId},
  };
  PayloadTracker tracker(
      &fake_clock, kShareTargetId, *container, attachment_payload_map,
      std::make_unique<PayloadTracker::PayloadUpdateQueue>(&task_runner));

  fake_clock.FastForward(absl::Seconds(4));
  std::optional<TransferMetadataBuilder> metadata_builder =
      tracker.ProcessPayloadUpdate(std::make_unique<PayloadTransferUpdate>(
          kSecondFileId, PayloadStatus::kSuccess, kFileSize,
          /*bytes_transferred=*/0));
  ASSERT_TRUE(metadata_builder.has_value());
  TransferMetadata metadata = metadata_builder->build();
  EXPECT_EQ(metadata.transfer_speed(), 0);
  EXPECT_EQ(metadata.estimated_time_remaining(), 0);
}

TEST(PayloadTrackerEdgeCasesTest, ZeroTotalSizeAndZeroSpeedClampsEtaToZero) {
  FakeClock fake_clock;
  TestSingleThreadExecutor task_runner(&fake_clock);
  std::unique_ptr<AttachmentContainer> container =
      AttachmentContainer::Builder()
          .AddFileAttachment(FileAttachment(
              kFileId, /*size=*/0, std::string(kFileName),
              std::string(kMimeType), service::proto::FileMetadata::IMAGE))
          .Build();
  absl::flat_hash_map<int64_t, int64_t> attachment_payload_map = {
      {kFileId, kFileId},
  };
  PayloadTracker tracker(
      &fake_clock, kShareTargetId, *container, attachment_payload_map,
      std::make_unique<PayloadTracker::PayloadUpdateQueue>(&task_runner));

  std::optional<TransferMetadataBuilder> metadata_builder =
      tracker.ProcessPayloadUpdate(std::make_unique<PayloadTransferUpdate>(
          kFileId, PayloadStatus::kInProgress, /*total_bytes=*/0,
          /*bytes_transferred=*/0));
  ASSERT_TRUE(metadata_builder.has_value());
  TransferMetadata metadata = metadata_builder->build();
  EXPECT_EQ(metadata.progress(), 100.0);
  EXPECT_EQ(metadata.transfer_speed(), 0);
  EXPECT_EQ(metadata.estimated_time_remaining(), 0);
}

TEST_F(PayloadTrackerTest, FirstWindowEtaUsesCurrentSpeedAndRoundsResult) {
  // First update initializes last_eta_update_timestamp_ because FakeClock
  // starts at absl::Now() while last_eta_update_timestamp_ defaults to
  // UnixEpoch().
  std::optional<TransferMetadata> metadata = PayloadUpdate(1024);
  ASSERT_TRUE(metadata.has_value());

  // Second update within the 3-second ETA window uses current_speed_ directly
  // via first_window_. Remaining bytes: 102400 - 21504 = 80896; speed:
  // 20480 / 2 = 10240 B/s; ETA: 80896 / 10240 = 7.9s -> rounds to 8s.
  FastForward(absl::Seconds(2));
  metadata = PayloadUpdate(21504);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->transfer_speed(), 10240);
  EXPECT_EQ(metadata->estimated_time_remaining(), 8);
}

TEST(PayloadTrackerEdgeCasesTest, OverflowingEtaClampsToUint64Max) {
  FakeClock fake_clock;
  TestSingleThreadExecutor task_runner(&fake_clock);
  constexpr int64_t kSecondFileId = 2;
  constexpr int64_t kThirdFileId = 3;
  constexpr int64_t kHugeFileSize = int64_t{1} << 62;
  std::unique_ptr<AttachmentContainer> container =
      AttachmentContainer::Builder()
          .AddFileAttachment(FileAttachment(
              kFileId, /*size=*/1, std::string(kFileName),
              std::string(kMimeType), service::proto::FileMetadata::IMAGE))
          .AddFileAttachment(FileAttachment(
              kSecondFileId, /*size=*/1, "second.jpg", std::string(kMimeType),
              service::proto::FileMetadata::IMAGE))
          .AddFileAttachment(FileAttachment(
              kThirdFileId, kHugeFileSize, "huge.bin", std::string(kMimeType),
              service::proto::FileMetadata::IMAGE))
          .Build();
  absl::flat_hash_map<int64_t, int64_t> attachment_payload_map = {
      {kFileId, kFileId},
      {kSecondFileId, kSecondFileId},
      {kThirdFileId, kThirdFileId},
  };
  PayloadTracker tracker(
      &fake_clock, kShareTargetId, *container, attachment_payload_map,
      std::make_unique<PayloadTracker::PayloadUpdateQueue>(&task_runner));

  ASSERT_TRUE(tracker
                  .ProcessPayloadUpdate(std::make_unique<PayloadTransferUpdate>(
                      kFileId, PayloadStatus::kSuccess, /*total_bytes=*/1,
                      /*bytes_transferred=*/1))
                  .has_value());

  fake_clock.FastForward(absl::Seconds(1000000000));
  std::optional<TransferMetadataBuilder> metadata_builder =
      tracker.ProcessPayloadUpdate(std::make_unique<PayloadTransferUpdate>(
          kSecondFileId, PayloadStatus::kSuccess, /*total_bytes=*/1,
          /*bytes_transferred=*/1));
  ASSERT_TRUE(metadata_builder.has_value());
  TransferMetadata metadata = metadata_builder->build();
  EXPECT_EQ(metadata.estimated_time_remaining(),
            std::numeric_limits<uint64_t>::max());
}

TEST(PayloadTrackerEdgeCasesTest, ZeroElapsedTimeGuardsSpeedAndEta) {
  FakeClock fake_clock;
  TestSingleThreadExecutor task_runner(&fake_clock);
  constexpr int64_t kSecondFileId = 2;
  std::unique_ptr<AttachmentContainer> container =
      AttachmentContainer::Builder()
          .AddFileAttachment(FileAttachment(
              kFileId, kFileSize, std::string(kFileName),
              std::string(kMimeType), service::proto::FileMetadata::IMAGE))
          .AddFileAttachment(FileAttachment(
              kSecondFileId, kFileSize, "second.jpg", std::string(kMimeType),
              service::proto::FileMetadata::IMAGE))
          .Build();
  absl::flat_hash_map<int64_t, int64_t> attachment_payload_map = {
      {kFileId, kFileId},
      {kSecondFileId, kSecondFileId},
  };
  PayloadTracker tracker(
      &fake_clock, kShareTargetId, *container, attachment_payload_map,
      std::make_unique<PayloadTracker::PayloadUpdateQueue>(&task_runner));

  // First update with 0 transferred bytes leaves current_speed_ == 0.0 and
  // initializes last_transfer_speed_update_timestamp_ and
  // last_eta_update_timestamp_ to fake_clock.Now().
  ASSERT_TRUE(tracker
                  .ProcessPayloadUpdate(std::make_unique<PayloadTransferUpdate>(
                      kFileId, PayloadStatus::kSuccess, kFileSize,
                      /*bytes_transferred=*/0))
                  .has_value());

  // Consecutive update at the exact same timestamp while current_speed_ == 0
  // hits the elapsed_seconds <= 0.0 guard.
  std::optional<TransferMetadataBuilder> metadata_builder =
      tracker.ProcessPayloadUpdate(std::make_unique<PayloadTransferUpdate>(
          kSecondFileId, PayloadStatus::kInProgress, kFileSize,
          /*bytes_transferred=*/2048));
  ASSERT_TRUE(metadata_builder.has_value());
  TransferMetadata metadata = metadata_builder->build();
  EXPECT_EQ(metadata.transfer_speed(), 0);
  EXPECT_EQ(metadata.estimated_time_remaining(), 0);
}

TEST(PayloadTrackerEdgeCasesTest, DecreasingTransferredSizeResetsSpeedToZero) {
  FakeClock fake_clock;
  TestSingleThreadExecutor task_runner(&fake_clock);
  constexpr int64_t kSecondFileId = 2;
  std::unique_ptr<AttachmentContainer> container =
      AttachmentContainer::Builder()
          .AddFileAttachment(FileAttachment(
              kFileId, kFileSize, std::string(kFileName),
              std::string(kMimeType), service::proto::FileMetadata::IMAGE))
          .AddFileAttachment(FileAttachment(
              kSecondFileId, kFileSize, "second.jpg", std::string(kMimeType),
              service::proto::FileMetadata::IMAGE))
          .Build();
  absl::flat_hash_map<int64_t, int64_t> attachment_payload_map = {
      {kFileId, kFileId},
      {kSecondFileId, kSecondFileId},
  };
  PayloadTracker tracker(
      &fake_clock, kShareTargetId, *container, attachment_payload_map,
      std::make_unique<PayloadTracker::PayloadUpdateQueue>(&task_runner));

  ASSERT_TRUE(tracker
                  .ProcessPayloadUpdate(std::make_unique<PayloadTransferUpdate>(
                      kFileId, PayloadStatus::kInProgress, kFileSize,
                      /*bytes_transferred=*/10240))
                  .has_value());

  fake_clock.FastForward(absl::Seconds(2));
  std::optional<TransferMetadataBuilder> metadata_builder =
      tracker.ProcessPayloadUpdate(std::make_unique<PayloadTransferUpdate>(
          kSecondFileId, PayloadStatus::kInProgress, kFileSize,
          /*bytes_transferred=*/4096));
  ASSERT_TRUE(metadata_builder.has_value());
  TransferMetadata metadata = metadata_builder->build();
  EXPECT_EQ(metadata.transfer_speed(), 0);
  EXPECT_EQ(metadata.estimated_time_remaining(), 0);
}

}  // namespace
}  // namespace nearby::sharing
