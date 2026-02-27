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

#include "sharing/common/nearby_share_prefs.h"

#include <string>

#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/pref_names.h"
#include "sharing/proto/enums.pb.h"

namespace nearby {
namespace sharing {
namespace prefs {
namespace {
using ::nearby::sharing::PrefNames;
using ::nearby::sharing::api::PreferenceManager;
using ::nearby::sharing::proto::DataUsage;
using ::nearby::sharing::proto::FastInitiationNotificationState;
}  // namespace

void RegisterNearbySharingPrefs(PreferenceManager& preference_manager,
                                bool skip_persistent_ones) {
  // These prefs are not synced across devices on purpose.

  if (!skip_persistent_ones) {
    // During logging out, we reset all settings and set these settings to new
    // values. To avoid setting them twice, we skip them here if
    // skip_persistent_ones is set to true.
    preference_manager.SetString(PrefNames::kCustomSavePath, std::string());
    preference_manager.SetInteger(PrefNames::kVisibility,
                                  static_cast<int>(kDefaultVisibility));
    preference_manager.SetInteger(PrefNames::kFallbackVisibility,
                                  static_cast<int>(kDefaultFallbackVisibility));
    preference_manager.SetBoolean(PrefNames::kIsAnalyticsEnabled, false);
  }

  preference_manager.SetInteger(
      PrefNames::kFastInitiationNotificationState,
      /*value=*/static_cast<int>(
          FastInitiationNotificationState::ENABLED_FAST_INIT));

  preference_manager.SetInteger(
      PrefNames::kDataUsage, static_cast<int>(DataUsage::WIFI_ONLY_DATA_USAGE));

  preference_manager.SetString(PrefNames::kDeviceId, std::string());

  preference_manager.SetString(PrefNames::kDeviceName, std::string());

  preference_manager.Remove(PrefNames::kPublicCertificateExpirationDict);
  preference_manager.Remove(PrefNames::kPrivateCertificateList);
  preference_manager.Remove(PrefNames::kSchedulerDownloadPublicCertificates);
  preference_manager.Remove(PrefNames::kSchedulerPrivateCertificateExpiration);
  preference_manager.Remove(PrefNames::kSchedulerPublicCertificateExpiration);
  preference_manager.Remove(PrefNames::kSchedulerUploadLocalDeviceCertificates);
  preference_manager.Remove(PrefNames::kUsers);
  preference_manager.SetBoolean(PrefNames::kAdvancedProtectionEnabled, false);
}

void ResetSchedulers(PreferenceManager& preference_manager) {
  preference_manager.Remove(PrefNames::kSchedulerDownloadPublicCertificates);
  preference_manager.Remove(PrefNames::kSchedulerPrivateCertificateExpiration);
  preference_manager.Remove(PrefNames::kSchedulerPublicCertificateExpiration);
  preference_manager.Remove(PrefNames::kSchedulerUploadLocalDeviceCertificates);
  preference_manager.Remove(PrefNames::kSchedulerGetAccountInfo);
}

}  // namespace prefs
}  // namespace sharing
}  // namespace nearby
