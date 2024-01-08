// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_UTILS_H_
#define THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_UTILS_H_

#include <string>

#include "absl/strings/string_view.h"
#include "sharing/internal/api/preference_manager.h"

namespace nearby::sharing {

// Converts the schedule preference to a readable string.
std::string ConvertToReadableSchedule(
    nearby::sharing::api::PreferenceManager& preference_manager,
    absl::string_view schedule_preference);

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_UTILS_H_
