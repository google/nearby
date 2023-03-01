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

#include "fastpair/repository/device_metadata.h"
namespace nearby {
namespace fastpair {
void FastPairNotificationController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FastPairNotificationController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FastPairNotificationController::NotifyShowDiscovery(
    DeviceMetadata& device) {
  for (Observer* observer : observers_) {
    observer->OnUpdateDevice(device);
  }
}

void FastPairNotificationController::ShowGuestDiscoveryNotification(
    DeviceMetadata& device) {
      NotifyShowDiscovery(device);
    }

}  // namespace fastpair
}  // namespace nearby
