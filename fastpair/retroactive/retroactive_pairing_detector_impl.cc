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
#include <memory>
#include <optional>
#include <utility>

#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/pairing/pairer_broker.h"

namespace nearby {
namespace fastpair {

RetroactivePairingDetectorImpl::RetroactivePairingDetectorImpl(
    Mediums& mediums, FastPairDeviceRepository* repository,
    SingleThreadExecutor* executor)
    : mediums_(mediums), repository_(repository), executor_(executor) {
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

  std::optional<FastPairDevice*> existing_device =
      repository_->FindDevice(device.GetMacAddress());
  if (existing_device.has_value()) {
    // Both classic paired and Fast paired devices call this function, so we
    // have to filter out pairing events for devices that we already know.
    return;
  }

  // In order to confirm that this device is a retroactive pairing, we need to
  // first check if it has already been saved to the user's account. If it has
  // already been saved, we don't want to prompt the user to save a device
  // again.
  // TODO(b/285047010): check if device has already been saved to the user's
  // account

  auto fast_pair_device =
      std::make_unique<FastPairDevice>(Protocol::kFastPairRetroactivePairing);
  fast_pair_device->SetPublicAddress(device.GetMacAddress());
  repository_->AddDevice(std::move(fast_pair_device));
  executor_->Execute("notify-retro-candidate",
                     [this, address = device.GetMacAddress()]() {
                       std::optional<FastPairDevice*> fast_pair_device =
                           repository_->FindDevice(address);
                       if (!fast_pair_device) return;
                       for (auto observer : observers_.GetObservers()) {
                         observer->OnRetroactivePairFound(**fast_pair_device);
                       }
                     });
}

}  // namespace fastpair
}  // namespace nearby
