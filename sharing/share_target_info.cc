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

#include <functional>
#include <string>
#include <utility>

#include "sharing/internal/public/logging.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"

namespace nearby {
namespace sharing {

ShareTargetInfo::ShareTargetInfo(
    std::string endpoint_id, const ShareTarget& share_target,
    std::function<void(const ShareTarget&, const TransferMetadata&)>
        transfer_update_callback)
    : endpoint_id_(std::move(endpoint_id)),
      self_share_(share_target.for_self_share),
      share_target_(share_target),
      transfer_update_callback_(std::move(transfer_update_callback)){}

ShareTargetInfo::ShareTargetInfo(ShareTargetInfo&&) = default;

ShareTargetInfo& ShareTargetInfo::operator=(ShareTargetInfo&&) = default;

ShareTargetInfo::~ShareTargetInfo() = default;

void ShareTargetInfo::set_share_target(const ShareTarget& share_target) {
  NL_DCHECK(share_target.id == share_target_.id);
  NL_DCHECK(share_target.for_self_share == share_target_.for_self_share);
  share_target_ = share_target;
}

void ShareTargetInfo::UpdateTransferMetadata(
    const TransferMetadata& transfer_metadata) {
  if (transfer_update_callback_) {
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
    transfer_update_callback_(share_target_, transfer_metadata);
  }
}

}  // namespace sharing
}  // namespace nearby
