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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAKE_FAST_PAIR_PRESENTER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAKE_FAST_PAIR_PRESENTER_H_

#include <memory>

#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "fastpair/ui/fast_pair/fast_pair_presenter_impl.h"

namespace nearby {
namespace fastpair {

class FakeFastPairPresenter : public FastPairPresenter {
 public:
  void ShowDiscovery(const FastPairDevice& device,
                     FastPairNotificationController& notification_controller,
                     DiscoveryCallback callback) override {
    show_discovery_ = true;
    callback(DiscoveryAction::kPairToDevice);
  }
  bool show_deiscovery() { return show_discovery_; }

 private:
  bool show_discovery_ = false;
};

class FakeFastPairPresenterFactory : public FastPairPresenterImpl::Factory {
 public:
  std::unique_ptr<FastPairPresenter> CreateInstance() override {
    auto fake_fast_pair_presenter = std::make_unique<FakeFastPairPresenter>();
    fake_fast_pair_presenter_ = fake_fast_pair_presenter.get();
    return fake_fast_pair_presenter;
  }

  FakeFastPairPresenter* fake_fast_pair_presenter() {
    return fake_fast_pair_presenter_;
  }

 protected:
  FakeFastPairPresenter* fake_fast_pair_presenter_ = nullptr;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAKE_FAST_PAIR_PRESENTER_H_
