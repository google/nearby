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

#include "fastpair/ui/fast_pair/fast_pair_presenter_impl.h"

#include <memory>
#include <utility>

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"

namespace nearby {
namespace fastpair {

// static
FastPairPresenterImpl::Factory*
    FastPairPresenterImpl::Factory::g_test_factory_ = nullptr;

// static
std::unique_ptr<FastPairPresenter> FastPairPresenterImpl::Factory::Create() {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance();
  }
  return std::make_unique<FastPairPresenterImpl>();
}

// static
void FastPairPresenterImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairPresenterImpl::Factory::~Factory() = default;

void FastPairPresenterImpl::ShowDiscovery(
    FastPairDevice& device,
    FastPairNotificationController& notification_controller,
    DiscoveryCallback callback) {
  notification_controller.ShowGuestDiscoveryNotification(device,
                                                         std::move(callback));
}

void FastPairPresenterImpl::ShowPairingResult(
    FastPairDevice& device,
    FastPairNotificationController& notification_controller, bool success) {
  notification_controller.ShowPairingResultNotification(device, success);
}
}  // namespace fastpair
}  // namespace nearby
