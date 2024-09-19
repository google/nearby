// Copyright 2021-2023 Google LLC
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

#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"

#include <stddef.h>

#include "sharing/internal/public/logging.h"

namespace nearby {
namespace sharing {

// LINT.IfChange(sharing_max_name_length_bytes)
const size_t kNearbyShareDeviceNameMaxLength = 32;
// LINT.ThenChange(
// //depot/google3/location/nearby/apps/better_together/windows/nearby_share/testing/functional/quick_share_transfer_without_login_api_test.py:sharing_max_name_length_bytes,
// //depot/google3/location/nearby/testing/nearby_native/client_windows/nearby_sharing_snippet.cc:sharing_max_name_length_bytes,
// //depot/google3/java/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/sharing/res/values/ints.xml:sharing_max_name_length_bytes,
// //depot/google3/javatests/com/google/android/gmscore/integ/modules/nearby/mobly/snippets/sharing/SharingClientSnippet.java:sharing_max_name_length_bytes)

NearbyShareLocalDeviceDataManager::NearbyShareLocalDeviceDataManager() =
    default;

NearbyShareLocalDeviceDataManager::~NearbyShareLocalDeviceDataManager() =
    default;

void NearbyShareLocalDeviceDataManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbyShareLocalDeviceDataManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NearbyShareLocalDeviceDataManager::Start() {
  if (is_running_) return;

  is_running_ = true;
  OnStart();
}

void NearbyShareLocalDeviceDataManager::Stop() {
  if (!is_running_) return;

  is_running_ = false;
  OnStop();
}

void NearbyShareLocalDeviceDataManager::NotifyLocalDeviceDataChanged(
    bool did_device_name_change, bool did_full_name_change,
    bool did_icon_change) {
  NL_LOG(INFO) << __func__ << ": did_device_name_change="
               << (did_device_name_change ? "true" : "false")
               << ", did_full_name_change="
               << (did_full_name_change ? "true" : "false")
               << ", did_icon_change=" << (did_icon_change ? "true" : "false");
  for (auto& observer : observers_.GetObservers()) {
    observer->OnLocalDeviceDataChanged(did_device_name_change,
                                       did_full_name_change, did_icon_change);
  }
}

}  // namespace sharing
}  // namespace nearby
