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


#ifndef THIRD_PARTY_NEARBY_PRESENCE_ACTION_TYPE_H_
#define THIRD_PARTY_NEARBY_PRESENCE_ACTION_TYPE_H_

namespace nearby {
namespace presence {

/**
  * Enum presentation of action case bits.
  */
enum ActionBits {
    kUnspecified = 0,
    kTimestampBit1 = 1 << 15,
    kTimestampBit2 = 1 << 14,
    kTimestampBit3 = 1 << 13,
    kTimestampBit4 = 1 << 12,
    kTapToTransfer = 1 << 11,
    kDtdiResrve1 = 1 << 10,
    kDtdiResrve2 = 1 << 9,
    kDtdiResrve3 = 1 << 8,
    kActiveUnlock = 1 << 7,
    kNearbyShare = 1 << 6,
    kFastPair = 1 << 5,
    kFitCast = 1 << 4,
    kPresenceManager = 1 << 3,
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_ACTION_TYPE_H_
