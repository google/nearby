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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FASTPAIR_PREFS_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FASTPAIR_PREFS_H_

#include "absl/base/attributes.h"
#include "internal/preferences/preferences_manager.h"

namespace nearby {
namespace fastpair {
namespace prefs {

ABSL_CONST_INIT extern const char kNearbyFastPairUsersName[];

void RegisterNearbyFastPairPrefs(
    preferences::PreferencesManager* preferences_manager);

}  // namespace prefs
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FASTPAIR_PREFS_H_
