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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_SERVICE_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_SERVICE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "fastpair/fast_pair_plugin.h"
#include "fastpair/fast_pair_seeker.h"
#include "fastpair/repository/fast_pair_device_repository.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

// Fast Pair Seeker Service. There should be only one instance of the service
// running on the system.
class FastPairService {
 public:
  FastPairService();
  ~FastPairService();

  // Registers a plugin provider. `name` must be a unique.
  // Returns an error if a provider with the same `name` is already registered.
  absl::Status RegisterPluginProvider(
      absl::string_view name, std::unique_ptr<FastPairPluginProvider> provider);

  // Unregisters a plugin provider and removes all plugins created by that
  // provider. Returns an error if there is no plugin provider registered with
  // `name`.
  absl::Status UnregisterPluginProvider(absl::string_view name);

  // Returns a `FastPairSeeker` implementation. The returned object has the
  // same lifetime as this `FastPairService` instance.
  FastPairSeeker* GetSeeker() const { return seeker_.get(); }

 private:
  void OnInitialDiscoveryEvent(const FastPairDevice& device,
                               InitialDiscoveryEvent event);
  void OnSubsequentDiscoveryEvent(const FastPairDevice& device,
                                  SubsequentDiscoveryEvent event);
  void OnPairEvent(const FastPairDevice& device, PairEvent event);
  void OnScreenEvent(const FastPairDevice& device, ScreenEvent event);
  void OnBatteryEvent(const FastPairDevice& device, BatteryEvent event);
  void OnRingEvent(const FastPairDevice& device, RingEvent event);
  SingleThreadExecutor executor_;
  std::unique_ptr<FastPairSeeker> seeker_;
  absl::flat_hash_map<std::string, std::unique_ptr<FastPairPluginProvider>>
      providers_;
  FastPairDeviceRepository devices_{&executor_};
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_SERVICE_H_
