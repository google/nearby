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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_UI_ACTIONS_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_UI_ACTIONS_H_

namespace nearby {
namespace fastpair {

enum class DiscoveryAction {
  kUnknown = 0,
  kPairToDevice = 1,
  kDismissedByUser = 2,
  kDismissedByOs = 3,
  kLearnMore = 4,
  kDone = 5,
  kDismissedByTimeout = 6,
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_UI_ACTIONS_H_
