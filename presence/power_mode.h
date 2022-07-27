// Copyright 2020 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_POWER_MODE_H_
#define THIRD_PARTY_NEARBY_PRESENCE_POWER_MODE_H_

namespace nearby {
namespace presence {

// High level concept of Power mode for Scan and Broadcast.
// More frequent, more power consumption, but less interval and latency.
// Native platforms would decide the specific interval based on their own
// configs.
enum class PowerMode {
  kNoPower = 0,
  kLowPower = 1,
  kBalanced = 2,
  kLowLatency = 3,
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_POWER_MODE_H_
