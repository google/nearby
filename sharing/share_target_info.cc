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

#include "sharing/share_target_info.h"

#include <string>
#include <utility>

#include "sharing/internal/public/logging.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"

namespace nearby {
namespace sharing {

ShareTargetInfo::ShareTargetInfo(
    std::string endpoint_id, const ShareTarget& share_target)
    : endpoint_id_(std::move(endpoint_id)),
      self_share_(share_target.for_self_share),
      share_target_(share_target) {}

ShareTargetInfo::ShareTargetInfo(ShareTargetInfo&&) = default;

ShareTargetInfo& ShareTargetInfo::operator=(ShareTargetInfo&&) = default;

ShareTargetInfo::~ShareTargetInfo() = default;

void ShareTargetInfo::UpdateTransferMetadata(
    const TransferMetadata& transfer_metadata) {
  if (got_final_status_) {
    // If we already got a final status, we can ignore any subsequent final
    // statuses caused by race conditions.
    NL_VLOG(1)
        << __func__ << ": Transfer update decorator swallowed "
        << "status update because a final status was already received: "
        << share_target_.id << ": "
        << TransferMetadata::StatusToString(transfer_metadata.status());
    return;
  }
  got_final_status_ = transfer_metadata.is_final_status();
  InvokeTransferUpdateCallback(transfer_metadata);
}

void ShareTargetInfo::set_disconnect_status(
    TransferMetadata::Status disconnect_status) {
  disconnect_status_ = disconnect_status;
  if (disconnect_status_ != TransferMetadata::Status::kUnknown &&
      !TransferMetadata::IsFinalStatus(disconnect_status_)) {
    NL_LOG(DFATAL) << "Disconnect status is not final: "
                   << static_cast<int>(disconnect_status_);
  }
}

void ShareTargetInfo::OnDisconnect() {
  if (disconnect_status_ != TransferMetadata::Status::kUnknown) {
    UpdateTransferMetadata(
        TransferMetadataBuilder().set_status(disconnect_status_).build());
  }
  connection_ = nullptr;
}

}  // namespace sharing
}  // namespace nearby
