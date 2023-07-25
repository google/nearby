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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_UI_UI_BROKER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_UI_UI_BROKER_H_

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/protocol.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"

namespace nearby {
namespace fastpair {

// The UIBroker is the entry point for the UI component in the FastPair system.
// It is responsible for brokering the 'show UI' calls to the correct Presenter
// implementation, and exposing user actions taken on that UI.
class UIBroker {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnDiscoveryAction(FastPairDevice& device,
                                   DiscoveryAction action) = 0;
  };

  virtual ~UIBroker() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual void ShowDiscovery(
      FastPairDevice& device,
      FastPairNotificationController& notification_controller) = 0;

  virtual void ShowPairingResult(
      FastPairDevice& device,
      FastPairNotificationController& notification_controller,
      bool success) = 0;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_UI_UI_BROKER_H_
