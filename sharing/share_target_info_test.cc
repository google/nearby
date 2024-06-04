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

#include <optional>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"

namespace nearby::sharing {
namespace {

constexpr absl::string_view kEndpointId = "12345";

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

 protected:
  void InvokeTransferUpdateCallback(
      const TransferMetadata& metadata) override {
    ++transfer_update_count_;
    last_transfer_metadata_ = metadata;
  }

 private:
  const bool is_incoming_;
  int transfer_update_count_ = 0;
  std::optional<TransferMetadata> last_transfer_metadata_;
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
