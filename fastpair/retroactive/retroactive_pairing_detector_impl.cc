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
#include <string>
#include <utility>

#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/repository/fast_pair_repository.h"
#include "internal/platform/implementation/account_manager.h"

namespace nearby {
namespace fastpair {

RetroactivePairingDetectorImpl::RetroactivePairingDetectorImpl(
    Mediums& mediums, FastPairDeviceRepository* repository,
    AccountManager* account_manager, SingleThreadExecutor* executor)
    : mediums_(mediums),
      repository_(repository),
      account_manager_(account_manager),
      executor_(executor) {
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
  if (existing_device.has_value() &&
      existing_device.value()->HasStartedPairing()) {
    // Both classic paired and Fast paired devices call this function, so we
    // have to filter out pairing events for device that paired from Fast Pair.
    NEARBY_LOGS(INFO) << __func__ << ": Ignoring Fast paired devices.";
    return;
  }

  // In order to confirm that this device is a retroactive pairing, we need to
  // first check if it has already been saved to the user's account. If it has
  // already been saved, we don't want to prompt the user to save a device
  // again.
  if (!account_manager_->GetCurrentAccount().has_value()) {
    NEARBY_LOGS(INFO) << __func__ << ": Ignoring because no logged in user.";
    return;
  }

  FastPairRepository::Get()->IsDeviceSavedToAccount(
      device.GetMacAddress(),
      [this, mac_address = device.GetMacAddress()](absl::Status status) {
        if (status.ok()) {
          NEARBY_LOGS(VERBOSE) << __func__
                               << ": Ignoring because device is already saved "
                                  "to the current account.";
          return;
        }
        NotifyRetroactiveDeviceFound(mac_address);
      });
}

void RetroactivePairingDetectorImpl::NotifyRetroactiveDeviceFound(
    absl::string_view mac_address) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": mac_address = " << mac_address;
  auto fast_pair_device =
      std::make_unique<FastPairDevice>(Protocol::kFastPairRetroactivePairing);
  fast_pair_device->SetPublicAddress(mac_address);
  repository_->AddDevice(std::move(fast_pair_device));
  executor_->Execute("notify-retro-candidate",
                     [this, address = std::string(mac_address)]() {
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
