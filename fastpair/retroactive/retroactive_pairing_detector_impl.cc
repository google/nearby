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

#include "fastpair/retroactive/retroactive_pairing_detector_impl.h"

#include <ios>

#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/pairing/pairer_broker.h"

namespace nearby {
namespace fastpair {

RetroactivePairingDetectorImpl::RetroactivePairingDetectorImpl(
    Mediums& mediums, PairerBroker* pairer_broker)
    : mediums_(mediums) {
  pairer_broker->AddObserver(this);
  mediums_.GetBluetoothClassic().AddObserver(this);
  mediums_.GetBluetoothClassic().StartDiscovery();
}

RetroactivePairingDetectorImpl::~RetroactivePairingDetectorImpl() {
  mediums_.GetBluetoothClassic().RemoveObserver(this);
  mediums_.GetBluetoothClassic().StopDiscovery();
}

void RetroactivePairingDetectorImpl::AddObserver(
    RetroactivePairingDetector::Observer* observer) {
  observers_.AddObserver(observer);
}

void RetroactivePairingDetectorImpl::RemoveObserver(
    RetroactivePairingDetector::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RetroactivePairingDetectorImpl::OnDevicePaired(FastPairDevice& device) {
  // The classic address is assigned to the Device during the
  // initial Fast Pair pairing protocol and if it doesn't exist,
  // then it wasn't properly paired during initial Fast Pair
  // pairing.
  if (!device.GetPublicAddress().has_value()) {
    return;
  }

  // The Bluetooth Adapter system event `DevicePairedChanged` fires before
  // Fast Pair's `OnDevicePaired`, and a Fast Pair pairing is expected to have
  // both events. If a device is Fast Paired, it is already inserted in the
  // |potential_retroactive_addresses_| in `DevicePairedChanged`; we need to
  // remove it to prevent a false positive.
  if (potential_retroactive_addresses_.contains(
          device.GetPublicAddress().value())) {
    NEARBY_LOGS(INFO)
        << __func__
        << ": paired with initial pairing, removing device at address = "
        << device.GetPublicAddress().value();
    potential_retroactive_addresses_.erase(device.GetPublicAddress().value());
  }
}

void RetroactivePairingDetectorImpl::DevicePairedChanged(
    BluetoothDevice& device, bool new_paired_status) {
  NEARBY_LOGS(INFO) << __func__
                    << " Device paired changed, name = " << device.GetName()
                    << " address = " << device.GetMacAddress()
                    << " new_paired_status = " << std::boolalpha
                    << new_paired_status;
  // This event fires whenever a device pairing has changed with
  // the BluetoothClassicMedium.
  // If the |new_paired_status| is false, it means a device was unpaired,
  // so we early return since it would not be a device to retroactively pair to.
  if (!new_paired_status) {
    return;
  }

  // Both classic paired and Fast paired devices call this function, so we
  // have to add the device to |potential_retroactive_addresses_|. We expect
  // devices paired via Fast Pair to always call `OnDevicePaired` after calling
  // this function, which will remove the device from
  // |potential_retroactive_addresses_|.
  potential_retroactive_addresses_.insert(device.GetMacAddress());

  // In order to confirm that this device is a retroactive pairing, we need to
  // first check if it has already been saved to the user's account. If it has
  // already been saved, we don't want to prompt the user to save a device
  // again.
  // TODO(b/285047010): check if device has already been saved to the user's
  // account

  // TODO(Janusz) Add implementation for AttemptRetroactivePairing
}

}  // namespace fastpair
}  // namespace nearby
