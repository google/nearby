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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_RETROACTIVE_RETROACTIVE_PAIRING_DETECTOR_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_RETROACTIVE_RETROACTIVE_PAIRING_DETECTOR_IMPL_H_

#include "absl/strings/string_view.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/repository/fast_pair_device_repository.h"
#include "fastpair/retroactive/retroactive_pairing_detector.h"
#include "internal/base/observer_list.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

class RetroactivePairingDetectorImpl
    : public RetroactivePairingDetector,
      public BluetoothClassicMedium ::Observer {
 public:
  RetroactivePairingDetectorImpl(Mediums& mediums,
                                 FastPairDeviceRepository* repository,
                                 AccountManager* account_manager,
                                 SingleThreadExecutor* executor);
  RetroactivePairingDetectorImpl(const RetroactivePairingDetectorImpl&) =
      delete;
  RetroactivePairingDetectorImpl& operator=(
      const RetroactivePairingDetectorImpl&) = delete;
  ~RetroactivePairingDetectorImpl() override;

  // RetroactivePairingDetector:
  void AddObserver(RetroactivePairingDetector::Observer* observer) override;
  void RemoveObserver(RetroactivePairingDetector::Observer* observer) override;

  // BluetoothClassicMedium::Observer
  void DevicePairedChanged(BluetoothDevice& device,
                           bool new_paired_status) override;

 private:
  void NotifyRetroactiveDeviceFound(absl::string_view mac_address);
  Mediums& mediums_;
  ObserverList<RetroactivePairingDetector::Observer> observers_;
  FastPairDeviceRepository* repository_;
  AccountManager* account_manager_;
  SingleThreadExecutor* executor_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_RETROACTIVE_RETROACTIVE_PAIRING_DETECTOR_IMPL_H_
