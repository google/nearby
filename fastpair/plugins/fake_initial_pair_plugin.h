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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_PLUGINS_INITIAL_PAIR_PLUGIN_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_PLUGINS_INITIAL_PAIR_PLUGIN_H_

#include <memory>

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/fast_pair_events.h"
#include "fastpair/fast_pair_plugin.h"
#include "fastpair/fast_pair_seeker.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

// Simple plugin that starts pairing when it sees a discoverable advertisement.
class FakeInitialPairPlugin : public FastPairPlugin {
 public:
  class Provider : public FastPairPluginProvider {
   public:
    std::unique_ptr<FastPairPlugin> GetPlugin(
        FastPairSeeker* seeker, const FastPairDevice* device) override {
      return std::make_unique<FakeInitialPairPlugin>(seeker, device);
    }
  };

  FakeInitialPairPlugin(FastPairSeeker* seeker, const FastPairDevice* device)
      : seeker_(seeker), device_(device) {}

  void OnInitialDiscoveryEvent(const InitialDiscoveryEvent& event) override {
    absl::Status status = seeker_->StartInitialPairing(
        *device_, InitialPairingParam{},
        {.on_pairing_result = [](const FastPairDevice& device,
                                 absl::Status status) {
          NEARBY_LOGS(INFO) << "Pairing result: " << status;
        }});
    NEARBY_LOGS(INFO) << "StartInitialPairing: " << status;
  }

 private:
  FastPairSeeker* seeker_;
  const FastPairDevice* device_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_PLUGINS_INITIAL_PAIR_PLUGIN_H_
