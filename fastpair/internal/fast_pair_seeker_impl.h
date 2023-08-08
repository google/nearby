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

#include "fastpair/fast_pair_controller.h"
#include "fastpair/fast_pair_events.h"
#include "fastpair/fast_pair_seeker.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/pairing/pairer_broker_impl.h"
#include "fastpair/repository/fast_pair_device_repository.h"
#include "fastpair/repository/fast_pair_repository.h"
#include "fastpair/retroactive/retroactive.h"
#include "fastpair/retroactive/retroactive_pairing_detector_impl.h"
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

  // Handle the state changes of screen lock.
  virtual void SetIsScreenLocked(bool is_locked) = 0;
  virtual void ForgetDeviceByAccountKey(const AccountKey& account_key) = 0;
};

class FastPairSeekerImpl : public FastPairSeekerExt,
                           ScannerBrokerImpl::Observer,
                           PairerBroker::Observer,
                           BluetoothClassicMedium::Observer,
                           RetroactivePairingDetector::Observer {
 public:
  struct ServiceCallbacks {
    absl::AnyInvocable<void(const FastPairDevice&, InitialDiscoveryEvent)>
        on_initial_discovery;
    absl::AnyInvocable<void(const FastPairDevice&, SubsequentDiscoveryEvent)>
        on_subsequent_discovery;
    absl::AnyInvocable<void(const FastPairDevice&, PairEvent)> on_pair_event;
    absl::AnyInvocable<void(ScreenEvent)> on_screen_event;
    absl::AnyInvocable<void(const FastPairDevice&, BatteryEvent)>
        on_battery_event;
    absl::AnyInvocable<void(const FastPairDevice&, RingEvent)> on_ring_event;
  };

  FastPairSeekerImpl(ServiceCallbacks callbacks, SingleThreadExecutor* executor,
                     AccountManager* account_manager,
                     FastPairDeviceRepository* devices,
                     FastPairRepository* repository);

  ~FastPairSeekerImpl() override;

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

  absl::Status FinishRetroactivePairing(
      const FastPairDevice& device, const FinishRetroactivePairingParam& param,
      PairingCallback callback) override;

  // From FastPairSeekerExt.
  absl::Status StartFastPairScan() override;
  absl::Status StopFastPairScan() override;
  void SetIsScreenLocked(bool is_locked) override;
  void ForgetDeviceByAccountKey(const AccountKey& account_key) override;

  // From BluetoothClassicMedium::Observer.
  void DeviceAdded(BluetoothDevice& device) override;
  void DeviceRemoved(BluetoothDevice& device) override;
  void DeviceAddressChanged(BluetoothDevice& device,
                            absl::string_view old_address) override;
  void DevicePairedChanged(BluetoothDevice& device,
                           bool new_paired_status) override;
  void DeviceConnectedStateChanged(BluetoothDevice& device,
                                   bool connected) override;

  // From RetroactivePairingDetector::Observer.
  void OnRetroactivePairFound(FastPairDevice& device) override;

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
  bool IsDeviceUnderPairing(const FastPairDevice& device);
  void FinishPairing(absl::Status result);

  ServiceCallbacks callbacks_;
  SingleThreadExecutor* executor_;
  AccountManager* account_manager_;
  FastPairDeviceRepository* devices_;
  FastPairRepository* repository_;
  Mediums mediums_;
  std::unique_ptr<ScannerBrokerImpl> scanner_;
  std::unique_ptr<ScannerBrokerImpl::ScanningSession> scanning_session_;
  std::unique_ptr<PairerBrokerImpl> pairer_broker_;
  std::unique_ptr<PairingCallback> pairing_callback_;
  std::unique_ptr<RetroactivePairingDetectorImpl> retro_detector_;
  std::unique_ptr<FastPairController> controller_;
  std::unique_ptr<Retroactive> retroactive_pair_;
  FastPairDevice* device_under_pairing_ = nullptr;
  FastPairDevice* test_device_ = nullptr;
  bool is_screen_locked_ = false;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_DEFAULT_FAST_PAIR_SEEKER_H_
