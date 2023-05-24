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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_PLUGIN_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_PLUGIN_H_

#include <memory>

#include "fastpair/fast_pair_events.h"
#include "fastpair/fast_pair_seeker.h"

namespace nearby {
namespace fastpair {

class FastPairPlugin {
 public:
  virtual ~FastPairPlugin() = default;

  // All functions below are synchronous.
  virtual void OnInitialDiscoveryEvent(const InitialDiscoveryEvent& event) {}
  virtual void OnSubsequentDiscoveryEvent(
      const SubsequentDiscoveryEvent& event) {}
  // Handles pair, unpair events.
  virtual void OnPairEvent(const PairEvent& event) {}
  virtual void OnScreenEvent(const ScreenEvent& event) {}
  virtual void OnBatteryEvent(const BatteryEvent& event) {}
  virtual void OnRingEvent(const RingEvent& event) {}
};

class FastPairPluginProvider {
 public:
  virtual ~FastPairPluginProvider() = default;

  // Returns an instance of `FastPairPlugin`.
  //
  // The plugin can use the `seeker` to communicate with the Fast Pair Seeker
  // Service. `seeker` is always non-null and has a lifetime at least as long as
  // the plugin returned by this call.
  // `device` is a non-null.
  virtual std::unique_ptr<FastPairPlugin> GetPlugin(
      FastPairSeeker* seeker, const FastPairDevice* device) = 0;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_PLUGIN_H_
