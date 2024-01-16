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

#include "absl/time/time.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/incoming_frames_reader.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/transfer_update_callback.h"

namespace nearby {
namespace sharing {

ShareTargetInfo::ShareTargetInfo() = default;

ShareTargetInfo::ShareTargetInfo(ShareTargetInfo&&) = default;

ShareTargetInfo& ShareTargetInfo::operator=(ShareTargetInfo&&) = default;

ShareTargetInfo::~ShareTargetInfo() = default;

}  // namespace sharing
}  // namespace nearby
