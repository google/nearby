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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_FAKE_FAST_PAIR_PLUGIN_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_FAKE_FAST_PAIR_PLUGIN_H_

#include <memory>

#include "absl/functional/any_invocable.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/fast_pair_events.h"
#include "fastpair/fast_pair_plugin.h"
#include "fastpair/fast_pair_seeker.h"

namespace nearby {
namespace fastpair {

class FakeFastPairPluginProvider;
class FakeFastPairPlugin : public FastPairPlugin {
 public:
  FakeFastPairPlugin(FastPairSeeker* seeker, const FastPairDevice* device,
                     FakeFastPairPluginProvider* provider)
      : seeker_(seeker), device_(device), provider_(provider) {}

  void OnInitialDiscoveryEvent(const InitialDiscoveryEvent& event) override;
  void OnScreenEvent(const ScreenEvent& event) override;

 private:
  FastPairSeeker* seeker_;
  const FastPairDevice* device_;
  FakeFastPairPluginProvider* provider_;
};

class FakeFastPairPluginProvider : public FastPairPluginProvider {
 public:
  std::unique_ptr<FastPairPlugin> GetPlugin(
      FastPairSeeker* seeker, const FastPairDevice* device) override {
    return std::make_unique<FakeFastPairPlugin>(seeker, device, this);
  }

  absl::AnyInvocable<void(const FastPairDevice* device,
                          const InitialDiscoveryEvent& event)>
      on_initial_discovery_event_ = nullptr;
  absl::AnyInvocable<void(const ScreenEvent& event)> on_screen_event_ = nullptr;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_FAKE_FAST_PAIR_PLUGIN_H_
