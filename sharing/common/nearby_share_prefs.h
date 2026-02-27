// Copyright 2021-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_COMMON_NEARBY_SHARE_PREFS_H_
#define THIRD_PARTY_NEARBY_SHARING_COMMON_NEARBY_SHARE_PREFS_H_

#include "absl/base/attributes.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/proto/enums.pb.h"

namespace nearby {
namespace sharing {
namespace prefs {

ABSL_CONST_INIT const proto::DeviceVisibility kDefaultVisibility =
    proto::DeviceVisibility::DEVICE_VISIBILITY_HIDDEN;
ABSL_CONST_INIT const proto::DeviceVisibility kDefaultFallbackVisibility =
    proto::DeviceVisibility::DEVICE_VISIBILITY_HIDDEN;
ABSL_CONST_INIT const int kDefaultMaxVisibilityExpirationSeconds =
    600;  // 10 minutes

void RegisterNearbySharingPrefs(
    nearby::sharing::api::PreferenceManager& preference_manager,
    bool skip_persistent_ones = false);

void ResetSchedulers(
    nearby::sharing::api::PreferenceManager& preference_manager);

}  // namespace prefs
}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_COMMON_NEARBY_SHARE_PREFS_H_
