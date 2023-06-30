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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_IMPL_H_

#include <memory>

#include "fastpair/common/device_metadata.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "fastpair/ui/fast_pair/fast_pair_presenter.h"

namespace nearby {
namespace fastpair {

class FastPairPresenterImpl : public FastPairPresenter {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairPresenter> Create();

    static void SetFactoryForTesting(Factory* g_test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<FastPairPresenter> CreateInstance() = 0;

   private:
    static Factory* g_test_factory_;
  };

  FastPairPresenterImpl() = default;
  FastPairPresenterImpl(const FastPairPresenterImpl&) = delete;
  FastPairPresenterImpl& operator=(const FastPairPresenterImpl&) = delete;

  void ShowDiscovery(FastPairDevice& device,
                     FastPairNotificationController& notification_controller,
                     DiscoveryCallback callback) override;
  void ShowPairingResult(
      FastPairDevice& device,
      FastPairNotificationController& notification_controller,
      bool success) override;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAST_PAIR_PRESENTER_IMPL_H_
