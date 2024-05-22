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

#include <functional>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"

namespace nearby::sharing {
namespace {
using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::IsTrue;
using testing::MockFunction;

constexpr absl::string_view kEndpointId = "12345";

class TestShareTargetInfo : public ShareTargetInfo {
 public:
  TestShareTargetInfo(
      std::string endpoint_id, const ShareTarget& share_target,
      std::function<void(const ShareTarget&, const TransferMetadata&)>
          transfer_update_callback)
      : ShareTargetInfo(std::move(endpoint_id), share_target,
                        std::move(transfer_update_callback)),
        is_incoming_(share_target.is_incoming) {}

  bool IsIncoming() const override { return is_incoming_; }

 private:
  const bool is_incoming_;
};

TEST(ShareTargetInfoTest, UpdateTransferMetadata) {
  MockFunction<void(const ShareTarget&, const TransferMetadata&)>
      update_callback;
  ShareTarget share_target;
  TestShareTargetInfo info(std::string(kEndpointId), share_target,
                           update_callback.AsStdFunction());

  EXPECT_CALL(update_callback, Call(_, _)).Times(2);

  info.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build());
  info.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build());
}

TEST(ShareTargetInfoTest, UpdateTransferMetadataAfterFinalStatus) {
  MockFunction<void(const ShareTarget&, const TransferMetadata&)>
      update_callback;
  ShareTarget share_target;
  TestShareTargetInfo info(std::string(kEndpointId), share_target,
                           update_callback.AsStdFunction());

  EXPECT_CALL(update_callback, Call(_, _));

  info.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kComplete)
          .build());
  info.UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .build());
}

TEST(ShareTargetInfoTest, SetDisconnectStatus) {
  MockFunction<void(const ShareTarget&, const TransferMetadata&)>
      update_callback;
  ShareTarget share_target;
  TestShareTargetInfo info(std::string(kEndpointId), share_target,
                           update_callback.AsStdFunction());

  info.set_disconnect_status(TransferMetadata::Status::kCancelled);
  EXPECT_EQ(info.disconnect_status(), TransferMetadata::Status::kCancelled);
}

TEST(ShareTargetInfoTest, OnDisconnect) {
  MockFunction<void(const ShareTarget&, const TransferMetadata&)>
      update_callback;
  ShareTarget share_target;
  TestShareTargetInfo info(std::string(kEndpointId), share_target,
                           update_callback.AsStdFunction());
  info.set_disconnect_status(TransferMetadata::Status::kCancelled);
  EXPECT_EQ(info.disconnect_status(), TransferMetadata::Status::kCancelled);
  EXPECT_CALL(update_callback, Call(_, _))
      .WillOnce(Invoke([](const ShareTarget& share_target,
                          const TransferMetadata& transfer_metadata) {
        EXPECT_THAT(transfer_metadata.status(),
                    Eq(TransferMetadata::Status::kCancelled));
        EXPECT_THAT(transfer_metadata.is_final_status(), IsTrue());
      }));

  info.OnDisconnect();
}

}  // namespace
}  // namespace nearby::sharing
