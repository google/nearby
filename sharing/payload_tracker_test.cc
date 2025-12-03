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
#include "internal/test/fake_task_runner.h"
#include "sharing/attachment_container.h"
#include "sharing/file_attachment.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/transfer_metadata.h"

namespace nearby {
namespace sharing {
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
    attachment_payload_map_.emplace(container_.GetFileAttachments()[0].id(),
                                 kFileId);
    auto payload_updates_queue =
        std::make_unique<PayloadTracker::PayloadUpdateQueue>(&task_runner_);
    payload_tracker_ = std::make_unique<PayloadTracker>(
        &fake_clock_, kShareTargetId, container_, attachment_payload_map_,
        std::move(payload_updates_queue));
  }

  void FastForward(absl::Duration duration) {
    fake_clock_.FastForward(duration);
  }

  std::optional<TransferMetadata> PayloadUpdate(int bytes_transferred) {
    auto transfer_update = std::make_unique<PayloadTransferUpdate>(
        /*payload_id=*/kFileId, PayloadStatus::kInProgress,
        /*total_bytes=*/kFileSize, /*bytes_transferred=*/bytes_transferred);
    return payload_tracker_->ProcessPayloadUpdate(std::move(transfer_update));
  }

 private:
  FakeClock fake_clock_;
  FakeTaskRunner task_runner_{&fake_clock_, 1};
  std::unique_ptr<PayloadTracker> payload_tracker_ = nullptr;
  AttachmentContainer container_;
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

}  // namespace
}  // namespace sharing
}  // namespace nearby
