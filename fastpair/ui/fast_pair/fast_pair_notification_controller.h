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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAST_PAIR_NOTIFICATION_CONTROLLER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAST_PAIR_NOTIFICATION_CONTROLLER_H_

#include "absl/functional/any_invocable.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/ui/actions.h"
#include "internal/base/observer_list.h"

namespace nearby {
namespace fastpair {

using DiscoveryCallback = absl::AnyInvocable<void(DiscoveryAction) const>;

enum class FastPairNotificationDismissReason {
  kDismissedByUser,
  kDismissedByOs,
  kDismissedByTimeout,
};
// This controller creates and manages messages for each FastPair corresponding
// notification event.
class FastPairNotificationController {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnUpdateDevice(FastPairDevice& device) = 0;
    virtual void OnPairingResult(FastPairDevice& device, bool success) = 0;
  };

  FastPairNotificationController() = default;
  ~FastPairNotificationController() = default;
  FastPairNotificationController(const FastPairNotificationController&) =
      delete;
  FastPairNotificationController& operator=(
      const FastPairNotificationController&) = delete;

  // Observer process
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void NotifyShowDiscovery(FastPairDevice& device);
  void NotifyShowPairingResult(FastPairDevice& device, bool success);

  // Creates and displays corresponding notification.
  void ShowGuestDiscoveryNotification(FastPairDevice& device,
                                      DiscoveryCallback callback);

  void ShowPairingResultNotification(FastPairDevice& device, bool success);

  // Triggers callback when the related action is clicked.
  void OnDiscoveryClicked(DiscoveryAction action);

 private:
  DiscoveryCallback callback_;
  ObserverList<Observer> observers_;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAST_PAIR_NOTIFICATION_CONTROLLER_H_
