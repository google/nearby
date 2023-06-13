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
#include <optional>
#include <utility>

#include "fastpair/fast_pair_events.h"
#include "fastpair/fast_pair_seeker.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/pairing/pairer_broker.h"
#include "fastpair/repository/fast_pair_device_repository.h"
#include "fastpair/scanning/scanner_broker_impl.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

// Fast Pair Seeker Extended interface.
// The methods in this interface are available only to FP internal plugins.
class FastPairSeekerExt : public FastPairSeeker {
 public:
  virtual absl::Status StartFastPairScan() = 0;
  virtual absl::Status StopFastPairScan() = 0;
};

class FastPairSeekerImpl : public FastPairSeekerExt,
                           ScannerBrokerImpl::Observer,
                           PairerBroker::Observer {
 public:
  struct ServiceCallbacks {
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

  FastPairSeekerImpl(ServiceCallbacks callbacks, SingleThreadExecutor* executor,
                     FastPairDeviceRepository* devices);

  // From FastPairSeeker.
  absl::Status StartInitialPairing(const FastPairDevice& device,
                                   const InitialPairingParam& params,
                                   PairingCallback callback) override;

  absl::Status StartSubsequentPairing(const FastPairDevice& device,
                                      const SubsequentPairingParam& params,
                                      PairingCallback callback) override;

  absl::Status StartRetroactivePairing(const FastPairDevice& device,
                                       const RetroactivePairingParam& param,
                                       PairingCallback callback) override;

  // From FastPairSeekerExt.
  absl::Status StartFastPairScan() override;
  absl::Status StopFastPairScan() override;

  // Handle the state changes of screen lock.
  void SetIsScreenLocked(bool is_locked);

  // Internal methods, not exported to plugins.
 private:
  // From ScannerBrokerImpl::Observer.
  void OnDeviceFound(FastPairDevice& device) override;
  void OnDeviceLost(FastPairDevice& device) override;

  // From PairerBroker:Observer
  void OnDevicePaired(FastPairDevice& device) override;
  void OnAccountKeyWrite(FastPairDevice& device,
                         std::optional<PairFailure> error) override;
  void OnPairingComplete(FastPairDevice& device) override;
  void OnPairFailure(FastPairDevice& device, PairFailure failure) override;

  void InvalidateScanningState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);

  ServiceCallbacks callbacks_;
  SingleThreadExecutor* executor_;
  FastPairDeviceRepository* devices_;
  Mediums mediums_;
  std::unique_ptr<ScannerBrokerImpl> scanner_;
  std::unique_ptr<ScannerBrokerImpl::ScanningSession> scanning_session_;
  std::unique_ptr<PairerBroker> pairer_broker_;
  FastPairDevice* test_device_ = nullptr;
  bool is_screen_locked_ = false;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_DEFAULT_FAST_PAIR_SEEKER_H_
