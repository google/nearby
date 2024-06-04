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

#include "sharing/wrapped_share_target_discovered_callback.h"

#include <cstdint>

#include "sharing/advertisement.h"
#include "sharing/internal/public/logging.h"
#include "sharing/share_target.h"

namespace nearby {
namespace sharing {

bool WrappedShareTargetDiscoveredCallback::ShouldBlockShareTarget(
    const ShareTarget& share_target) const {
  return blocked_vendor_id_ != Advertisement::BlockedVendorId::kNone &&
         share_target.vendor_id == static_cast<uint8_t>(blocked_vendor_id_);
}

void WrappedShareTargetDiscoveredCallback::OnShareTargetDiscovered(
    const ShareTarget& share_target) {
  if (ShouldBlockShareTarget(share_target)) {
    NL_LOG(INFO) << "Skipping share target discovered for vendor id "
                 << static_cast<uint32_t>(blocked_vendor_id_);
    return;
  }
  if (callback_ != nullptr) {
    callback_->OnShareTargetDiscovered(share_target);
  }
}

void WrappedShareTargetDiscoveredCallback::OnShareTargetUpdated(
    const ShareTarget& share_target) {
  if (ShouldBlockShareTarget(share_target)) {
    NL_LOG(INFO) << "Skipping share target updated for vendor id "
                 << static_cast<uint32_t>(blocked_vendor_id_);
    return;
  }
  if (callback_ != nullptr) {
    callback_->OnShareTargetUpdated(share_target);
  }
}

void WrappedShareTargetDiscoveredCallback::OnShareTargetLost(
    const ShareTarget& share_target) {
  if (ShouldBlockShareTarget(share_target)) {
    NL_LOG(INFO) << "Skipping share target lost for vendor id "
                 << static_cast<uint32_t>(blocked_vendor_id_);
    return;
  }
  if (callback_ != nullptr) {
    callback_->OnShareTargetLost(share_target);
  }
}

}  // namespace sharing
}  // namespace nearby
