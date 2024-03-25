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

#include "sharing/incoming_share_target_info.h"

#include <functional>
#include <string>
#include <utility>

#include "sharing/share_target.h"
#include "sharing/share_target_info.h"
#include "sharing/transfer_metadata.h"

namespace nearby {
namespace sharing {

IncomingShareTargetInfo::IncomingShareTargetInfo(
    std::string endpoint_id, const ShareTarget& share_target,
    std::function<void(const ShareTarget&, const TransferMetadata&)>
        transfer_update_callback)
    : ShareTargetInfo(std::move(endpoint_id), share_target,
                      std::move(transfer_update_callback)) {}

IncomingShareTargetInfo::IncomingShareTargetInfo(IncomingShareTargetInfo&&) =
    default;

IncomingShareTargetInfo& IncomingShareTargetInfo::operator=(
    IncomingShareTargetInfo&&) = default;

IncomingShareTargetInfo::~IncomingShareTargetInfo() = default;

}  // namespace sharing
}  // namespace nearby
