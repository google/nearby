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

#include "absl/strings/string_view.h"
#include "fastpair/repository/device_metadata.h"
#include "fastpair/server_access/fast_pair_repository.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

void FastPairPresenterImpl::ShowDiscovery(
    const FastPairDevice& device,
    FastPairNotificationController& notification_controller) {
  FastPairRepository::Get()->GetDeviceMetadata(
      device.model_id,
      [&notification_controller, this](const DeviceMetadata& device_metadata) {
        NEARBY_LOGS(INFO) << __func__
                          << "Retrieved metadata to notification controller.";
        FastPairPresenterImpl::OnDiscoveryMetadataRetrieved(
            device_metadata, notification_controller);
      });
}

void FastPairPresenterImpl::OnDiscoveryMetadataRetrieved(
    const DeviceMetadata& device_metadata,
    FastPairNotificationController& notification_controller) {
  notification_controller.ShowGuestDiscoveryNotification(device_metadata);
}

}  // namespace fastpair
}  // namespace nearby
