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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_MOCK_FAST_PAIR_NOTIFICATION_CONTROLLER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_MOCK_FAST_PAIR_NOTIFICATION_CONTROLLER_H_

#include "gmock/gmock.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"

namespace nearby {
namespace fastpair {
class MockFastPairNotificationController
    : public FastPairNotificationController {
 public:
  MOCK_METHOD(void, ShowGuestDiscoveryNotification,
              (FastPairDevice & device, DiscoveryCallback));
  MOCK_METHOD(void, OnDiscoveryClicked, (DiscoveryAction));
  MOCK_METHOD(void, AddObserver, (Observer*));
  MOCK_METHOD(void, RemoveObserver, (Observer*));

  void NotifyShowDiscovery(FastPairDevice& device) {
    for (Observer* observer : observers_.GetObservers()) {
      observer->OnUpdateDevice(device);
    }
  }

 private:
  ObserverList<Observer> observers_;
};
}  // namespace fastpair
}  // namespace nearby
#endif  // THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_MOCK_FAST_PAIR_NOTIFICATION_CONTROLLER_H_
