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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_H_

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"

namespace nearby {
namespace fastpair {

// This Presenter creates and manages UI component with Notification Controller.
class FastPairPresenter {
 public:
  virtual void ShowDiscovery(
      FastPairDevice& device,
      FastPairNotificationController& notification_controller,
      DiscoveryCallback callback) = 0;
  virtual void ShowPairingResult(
      FastPairDevice& device,
      FastPairNotificationController& notification_controller,
      bool success) = 0;

  virtual ~FastPairPresenter() = default;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_H_
