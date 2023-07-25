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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_UI_BROKER_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_UI_BROKER_IMPL_H_

#include <memory>

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fast_pair_presenter.h"
#include "fastpair/ui/ui_broker.h"
#include "internal/base/observer_list.h"

namespace nearby {
namespace fastpair {

class UIBrokerImpl : public UIBroker {
 public:
  UIBrokerImpl();
  UIBrokerImpl(const UIBrokerImpl &) = delete;
  UIBrokerImpl &operator=(const UIBrokerImpl &) = delete;

  void AddObserver(Observer *observer) override;
  void RemoveObserver(Observer *observer) override;
  void ShowDiscovery(
      FastPairDevice &device,
      FastPairNotificationController &notification_controller) override;
  void ShowPairingResult(
      FastPairDevice &device,
      FastPairNotificationController &notification_controller,
      bool success) override;

 private:
  void NotifyDiscoveryAction(FastPairDevice &device, DiscoveryAction action);

  std::unique_ptr<FastPairPresenter> fast_pair_presenter_;
  ObserverList<Observer> observers_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_UI_BROKER_IMPL_H_
