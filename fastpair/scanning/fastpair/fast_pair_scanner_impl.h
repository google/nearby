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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_SCANNER_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_SCANNER_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner.h"
#include "internal/base/observer_list.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/timer_impl.h"

namespace nearby {
namespace fastpair {

class FastPairScannerImpl : public FastPairScanner {
 public:
  explicit FastPairScannerImpl(Mediums& mediums,
                               SingleThreadExecutor* executor);
  FastPairScannerImpl(const FastPairScannerImpl&) = delete;
  FastPairScannerImpl& operator=(const FastPairScannerImpl&) = delete;
  ~FastPairScannerImpl() override = default;

  // FastPairScanner::Observer
  void AddObserver(FastPairScanner::Observer* observer) override;
  void RemoveObserver(FastPairScanner::Observer* observer) override;

  // Fast Pair discovered peripheral callback
  void OnDeviceFound(const BlePeripheral& peripheral);
  void OnDeviceLost(const BlePeripheral& peripheral);

  void NotifyDeviceFound(const BlePeripheral& peripheral);
  // Todo(b/267348348): Support Flags to control feature ramp
  bool IsFastPairLowPowerEnabled() const { return false; }

  std::unique_ptr<ScanningSession> StartScanning() override;
  void StopScanning();

 private:
  void StartScanningInternal() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  // Pauses, and then restarts, scanning for a few seconds to safe power.
  void PauseScanning() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  void StartTimer(absl::Duration delay, absl::AnyInvocable<void()> callback)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);

  Mediums& mediums_;
  SingleThreadExecutor* executor_;
  std::unique_ptr<TimerImpl> timer_ ABSL_GUARDED_BY(*executor_);

  // Map of a Bluetooth device address to a set of advertisement data we have
  // seen.
  absl::flat_hash_map<std::string, std::set<std::string>>
      device_address_advertisement_data_map_;
  ObserverList<FastPairScanner::Observer> observer_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_SCANNER_IMPL_H_
