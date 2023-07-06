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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_EVENTS_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_EVENTS_H_

#include <optional>
namespace nearby {
namespace fastpair {

// Fast Pair plugins receive notifications from the FP service about various
// system events. The structures below provide details about the events.

struct InitialDiscoveryEvent {};

struct SubsequentDiscoveryEvent {};

struct PairEvent {
  bool is_paired;
};

struct ScreenEvent {
  std::optional<bool> is_locked;
};

struct BatteryEvent {};

struct RingEvent {};

}  // namespace fastpair
}  // namespace nearby
#endif  // THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_EVENTS_H_
