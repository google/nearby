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
#include <string>

#include "absl/container/flat_hash_set.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/pairing/pairer_broker.h"
#include "fastpair/retroactive/retroactive_pairing_detector.h"
#include "internal/base/observer_list.h"
#include "internal/platform/bluetooth_classic.h"

namespace nearby {
namespace fastpair {

class RetroactivePairingDetectorImpl : public RetroactivePairingDetector,
                                       public BluetoothClassicMedium ::Observer,
                                       public PairerBroker::Observer {
 public:
  RetroactivePairingDetectorImpl(Mediums& mediums, PairerBroker* pairer_broker);
  RetroactivePairingDetectorImpl(const RetroactivePairingDetectorImpl&) =
      delete;
  RetroactivePairingDetectorImpl& operator=(
      const RetroactivePairingDetectorImpl&) = delete;
  ~RetroactivePairingDetectorImpl() override;

  // RetroactivePairingDetector:
  void AddObserver(RetroactivePairingDetector::Observer* observer) override;
  void RemoveObserver(RetroactivePairingDetector::Observer* observer) override;

  // BluetoothClassicMedium :: Observer
  void DevicePairedChanged(BluetoothDevice& device,
                           bool new_paired_status) override;

  // PairerBroker::Observer
  void OnDevicePaired(FastPairDevice& device) override;

 private:
  Mediums& mediums_;
  ObserverList<RetroactivePairingDetector::Observer> observers_;
  absl::flat_hash_set<std::string> potential_retroactive_addresses_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_RETROACTIVE_RETROACTIVE_PAIRING_DETECTOR_IMPL_H_
