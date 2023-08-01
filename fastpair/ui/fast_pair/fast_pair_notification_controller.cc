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

#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"

#include <utility>

#include "absl/functional/any_invocable.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/ui/actions.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
void FastPairNotificationController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FastPairNotificationController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FastPairNotificationController::NotifyShowDiscovery(
    FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__;
  for (Observer* observer : observers_.GetObservers()) {
    observer->OnUpdateDevice(device);
  }
}

void FastPairNotificationController::NotifyShowPairingResult(
    FastPairDevice& device, bool success) {
  NEARBY_LOGS(INFO) << __func__;
  for (Observer* observer : observers_.GetObservers()) {
    observer->OnPairingResult(device, success);
  }
}

void FastPairNotificationController::ShowGuestDiscoveryNotification(
    FastPairDevice& device, DiscoveryCallback callback) {
  callback_ = std::move(callback);
  NEARBY_LOGS(INFO) << __func__ << "Notify show guest discovery notification. ";
  NotifyShowDiscovery(device);
}

void FastPairNotificationController::ShowPairingResultNotification(
    FastPairDevice& device, bool success) {
  NEARBY_LOGS(INFO) << __func__ << "Notify show pairing result notification. ";
  NotifyShowPairingResult(device, success);
}

void FastPairNotificationController::OnDiscoveryClicked(
    DiscoveryAction action) {
  NEARBY_LOGS(INFO) << __func__
                    << "Discovery action button is clicked in the app.";
  DCHECK(callback_);
  callback_(action);
}

}  // namespace fastpair
}  // namespace nearby
