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
#include "sharing/attachment_info.h"
#include "sharing/file_attachment.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"

namespace nearby {
namespace sharing {
namespace {

constexpr int64_t kFileId = 1;
constexpr int64_t kFileSize = 100 * 1024;  // 100KB
constexpr absl::string_view kFileName = "test.jpg";
constexpr absl::string_view kMimeType = "image/jpg";

class PayloadTrackerTest : public ::testing::Test {
 public:
  void SetUp() override {
    share_target_.file_attachments.clear();
    share_target_.file_attachments.push_back(FileAttachment(
        kFileId, kFileSize, std::string(kFileName), std::string(kMimeType),
        service::proto::FileMetadata::IMAGE));
    attachment_info_map_.clear();
    AttachmentInfo attachment_info;
    attachment_info.payload_id = kFileId;
    attachment_info_map_.emplace(share_target_.file_attachments.at(0).id(),
                                 std::move(attachment_info));
    payload_tracker_ = std::make_unique<PayloadTracker>(
        context(), share_target_, attachment_info_map_,
        [&](ShareTarget share_target, TransferMetadata transfer_metadata) {
          current_percentage_ = transfer_metadata.progress();
        });
  }

  float percentage() const { return current_percentage_; }

  void FastForward(absl::Duration duration) {
    FakeClock* clock = dynamic_cast<FakeClock*>(context()->GetClock());
    clock->FastForward(duration);
  }

  void PayloadUpdate(int bytes_transferred) {
    auto transfer_update = std::make_unique<PayloadTransferUpdate>(
        /*payload_id=*/kFileId, PayloadStatus::kInProgress,
        /*total_bytes=*/kFileSize, /*bytes_transferred=*/bytes_transferred);
    payload_tracker_->OnStatusUpdate(std::move(transfer_update), std::nullopt);
  }

 private:
  Context* context() {
    static FakeContext* context = new FakeContext();
    return context;
  }

  std::unique_ptr<PayloadTracker> payload_tracker_ = nullptr;
  float current_percentage_ = 0.0;
  ShareTarget share_target_;
  absl::flat_hash_map<int64_t, AttachmentInfo> attachment_info_map_;
};

TEST_F(PayloadTrackerTest, StatusUpdateWithoutTimeUpdate) {
  EXPECT_EQ(percentage(), 0.0);
  PayloadUpdate(1024);
  EXPECT_EQ(percentage(), 1.0);
  PayloadUpdate(2048);
  EXPECT_EQ(percentage(), 2.0);
}

TEST_F(PayloadTrackerTest, StatusUpdateWithTimeUpdate) {
  EXPECT_EQ(percentage(), 0.0);
  PayloadUpdate(1024);
  EXPECT_EQ(percentage(), 1.0);
  FastForward(absl::Milliseconds(100));
  PayloadUpdate(2048);
  EXPECT_EQ(percentage(), 2.0);
  FastForward(absl::Milliseconds(100));
  PayloadUpdate(3072);
  EXPECT_EQ(percentage(), 3.0);
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
