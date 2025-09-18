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

#include "absl/base/attributes.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/pref_names.h"
#include "sharing/proto/enums.pb.h"

namespace nearby {
namespace sharing {
namespace prefs {
namespace {
using ::nearby::sharing::PrefNames;
using ::nearby::sharing::api::PreferenceManager;

using DataUsage = ::nearby::sharing::proto::DataUsage;
using FastInitiationNotificationState =
    ::nearby::sharing::proto::FastInitiationNotificationState;
}  // namespace

ABSL_CONST_INIT const char* kNearbySharingBackgroundVisibilityName =
    PrefNames::kVisibility.data();
ABSL_CONST_INIT const char* kNearbySharingBackgroundFallbackVisibilityName =
    PrefNames::kFallbackVisibility.data();
ABSL_CONST_INIT const char*
    kNearbySharingBackgroundVisibilityExpirationSeconds =
        PrefNames::kVisibilityExpirationSeconds.data();
ABSL_CONST_INIT const char* kNearbySharingCustomSavePath =
    PrefNames::kCustomSavePath.data();
ABSL_CONST_INIT const char* kNearbySharingDataUsageName =
    PrefNames::kDataUsage.data();
ABSL_CONST_INIT const char* kNearbySharingDeviceIdName =
    PrefNames::kDeviceId.data();
ABSL_CONST_INIT const char* kNearbySharingDeviceNameName =
    PrefNames::kDeviceName.data();
ABSL_CONST_INIT const char* kNearbySharingFastInitiationNotificationStateName =
    PrefNames::kFastInitiationNotificationState.data();
ABSL_CONST_INIT const char* kNearbySharingPrivateCertificateListName =
    PrefNames::kPrivateCertificateList.data();
ABSL_CONST_INIT const char* kNearbySharingPublicCertificateExpirationDictName =
    PrefNames::kPublicCertificateExpirationDict.data();
ABSL_CONST_INIT const char*
    kNearbySharingSchedulerDownloadPublicCertificatesName =
        PrefNames::kSchedulerDownloadPublicCertificates.data();
ABSL_CONST_INIT const char*
    kNearbySharingSchedulerPrivateCertificateExpirationName =
        PrefNames::kSchedulerPrivateCertificateExpiration.data();
ABSL_CONST_INIT const char*
    kNearbySharingSchedulerPublicCertificateExpirationName =
        PrefNames::kSchedulerPublicCertificateExpiration.data();
ABSL_CONST_INIT const char*
    kNearbySharingSchedulerUploadLocalDeviceCertificatesName =
        PrefNames::kSchedulerUploadLocalDeviceCertificates.data();
ABSL_CONST_INIT const char* kNearbySharingUsersName = PrefNames::kUsers.data();
ABSL_CONST_INIT const char* kNearbySharingIsAnalyticsEnabledName =
    PrefNames::kIsAnalyticsEnabled.data();

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
}

}  // namespace prefs
}  // namespace sharing
}  // namespace nearby
