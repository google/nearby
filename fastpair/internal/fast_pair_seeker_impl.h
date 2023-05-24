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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_DEFAULT_FAST_PAIR_SEEKER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_DEFAULT_FAST_PAIR_SEEKER_H_

#include <memory>
#include <utility>

#include "fastpair/fast_pair_events.h"
#include "fastpair/fast_pair_seeker.h"

namespace nearby {
namespace fastpair {

// Fast Pair Seeker Extended interface.
// The methods in this interface are available only to FP internal plugins.
class FastPairSeekerExt : public FastPairSeeker {
 public:
  virtual absl::Status StartFastPairScan() = 0;
  virtual absl::Status StopFastPairScan() = 0;
};

class FastPairSeekerImpl : public FastPairSeekerExt {
 public:
  struct ServiceCallbacks {
    absl::AnyInvocable<void(std::unique_ptr<FastPairDevice>)> on_device_added;
    absl::AnyInvocable<void(const FastPairDevice&)> on_device_lost;

    absl::AnyInvocable<void(const FastPairDevice&, InitialDiscoveryEvent)>
        on_initial_discovery;
    absl::AnyInvocable<void(const FastPairDevice&, SubsequentDiscoveryEvent)>
        on_subsequent_discovery;
    absl::AnyInvocable<void(const FastPairDevice&, PairEvent)> on_pair_event;
    absl::AnyInvocable<void(const FastPairDevice&, ScreenEvent)>
        on_screen_event;
    absl::AnyInvocable<void(const FastPairDevice&, BatteryEvent)>
        on_battery_event;
    absl::AnyInvocable<void(const FastPairDevice&, RingEvent)> on_ring_event;
  };

  explicit FastPairSeekerImpl(ServiceCallbacks callbacks)
      : callbacks_(std::move(callbacks)) {}

  // From FastPairSeeker.
  absl::Status StartInitialPairing(FastPairDevice& device,
                                   const InitialPairingParam& params,
                                   PairingCallback callback) override;

  absl::Status StartSubsequentPairing(FastPairDevice& device,
                                      const SubsequentPairingParam& params,
                                      PairingCallback callback) override;

  absl::Status StartRetroactivePairing(FastPairDevice& device,
                                       const RetroactivePairingParam& param,
                                       PairingCallback callback) override;

  // From FastPairSeekerExt
  absl::Status StartFastPairScan() override;
  absl::Status StopFastPairScan() override;

  // Internal methods, not exported to plugins.

 private:
  ServiceCallbacks callbacks_;
  FastPairDevice* test_device_ = nullptr;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_DEFAULT_FAST_PAIR_SEEKER_H_
