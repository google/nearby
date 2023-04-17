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

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "fastpair/repository/device_metadata.h"
#include "fastpair/server_access/fast_pair_repository.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "internal/platform/logging.h"

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
    const FastPairDevice& device,
    FastPairNotificationController& notification_controller,
    DiscoveryCallback callback) {
  callback_ = std::move(callback);
  FastPairRepository::Get()->GetDeviceMetadata(
      device.GetModelId(), [&device, &notification_controller,
                            this](const DeviceMetadata& device_metadata) {
        NEARBY_LOGS(INFO) << __func__
                          << "Retrieved metadata to notification controller.";
        OnDiscoveryMetadataRetrieved(device, device_metadata,
                                     notification_controller);
      });
}

void FastPairPresenterImpl::OnDiscoveryMetadataRetrieved(
    const FastPairDevice& device, const DeviceMetadata& device_metadata,
    FastPairNotificationController& notification_controller) {
  notification_controller.ShowGuestDiscoveryNotification(device_metadata,
                                                         std::move(callback_));
}
}  // namespace fastpair
}  // namespace nearby
