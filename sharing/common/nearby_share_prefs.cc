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
#include <vector>

#include "absl/base/attributes.h"
#include "absl/time/clock.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/proto/enums.pb.h"

namespace nearby {
namespace sharing {
namespace prefs {
namespace {
using ::nearby::sharing::api::PreferenceManager;

using DataUsage = ::nearby::sharing::proto::DataUsage;
using FastInitiationNotificationState =
    ::nearby::sharing::proto::FastInitiationNotificationState;
}  // namespace

ABSL_CONST_INIT const char kNearbySharingAllowedContactsName[] =
    "nearby_sharing.allowed_contacts";
ABSL_CONST_INIT const char kNearbySharingBackgroundVisibilityName[] =
    "nearby_sharing.background_visibility";
ABSL_CONST_INIT const char kNearbySharingBackgroundFallbackVisibilityName[] =
    "nearby_sharing.background_fallback_visibility";
ABSL_CONST_INIT const char
    kNearbySharingBackgroundVisibilityExpirationSeconds[] =
        "nearby_sharing.background_visibility_expiration_seconds";
ABSL_CONST_INIT const char kNearbySharingContactUploadHashName[] =
    "nearby_sharing.contact_upload_hash";
ABSL_CONST_INIT const char kNearbySharingContactUploadTimeName[] =
    "nearby_sharing.contact_upload_time";
ABSL_CONST_INIT const char kNearbySharingCustomSavePath[] =
    "nearby_sharing.custom_save_path";
ABSL_CONST_INIT const char kNearbySharingDataUsageName[] =
    "nearby_sharing.data_usage";
ABSL_CONST_INIT const char kNearbySharingDeviceIdName[] =
    "nearby_sharing.device_id";
ABSL_CONST_INIT const char kNearbySharingDeviceNameName[] =
    "nearby_sharing.device_name";
ABSL_CONST_INIT const char kNearbySharingFastInitiationNotificationStateName[] =
    "nearby_sharing.fast_initiation_notification_state";
ABSL_CONST_INIT const char kNearbySharingFullNameName[] =
    "nearby_sharing.full_name";
ABSL_CONST_INIT const char kNearbySharingIconUrlName[] =
    "nearby_sharing.icon_url";
ABSL_CONST_INIT const char kNearbySharingIconTokenName[] =
    "nearby_sharing.icon_token";
ABSL_CONST_INIT const char kNearbySharingPublicCertificateExpirationDictName[] =
    "nearbyshare.public_certificate_expiration_dict";
ABSL_CONST_INIT const char kNearbySharingPrivateCertificateListName[] =
    "nearbyshare.private_certificate_list";
ABSL_CONST_INIT const char
    kNearbySharingSchedulerContactDownloadAndUploadName[] =
        "nearby_sharing.scheduler.contact_download_and_upload";
ABSL_CONST_INIT const char kNearbySharingSchedulerDownloadDeviceDataName[] =
    "nearby_sharing.scheduler.download_device_data";
ABSL_CONST_INIT const char
    kNearbySharingSchedulerDownloadPublicCertificatesName[] =
        "nearby_sharing.scheduler.download_public_certificates";
ABSL_CONST_INIT const char
    kNearbySharingSchedulerPrivateCertificateExpirationName[] =
        "nearby_sharing.scheduler.private_certificate_expiration";
ABSL_CONST_INIT const char
    kNearbySharingSchedulerPublicCertificateExpirationName[] =
        "nearby_sharing.scheduler.public_certificate_expiration";
ABSL_CONST_INIT const char kNearbySharingSchedulerUploadDeviceNameName[] =
    "nearby_sharing.scheduler.upload_device_name";
ABSL_CONST_INIT const char
    kNearbySharingSchedulerUploadLocalDeviceCertificatesName[] =
        "nearby_sharing.scheduler.upload_local_device_certificates";
ABSL_CONST_INIT const char kNearbySharingUsersName[] = "nearby_sharing.users";
ABSL_CONST_INIT const char kNearbySharingIsAnalyticsEnabledName[] =
    "nearby_sharing.is_analytics_enabled";
ABSL_CONST_INIT const char kNearbySharingIsAllContactsEnabledName[] =
    "nearby_sharing.is_all_contacts_enabled";

void RegisterNearbySharingPrefs(PreferenceManager& preference_manager,
                                bool skip_persistent_ones) {
  // These prefs are not synced across devices on purpose.

  if (!skip_persistent_ones) {
    // During logging out, we reset all settings and set these settings to new
    // values. To avoid setting them twice, we skip them here if
    // skip_persistent_ones is set to true.
    preference_manager.SetString(kNearbySharingCustomSavePath, std::string());
    preference_manager.SetInteger(kNearbySharingBackgroundVisibilityName,
                                  static_cast<int>(kDefaultVisibility));
    preference_manager.SetInteger(
        kNearbySharingBackgroundFallbackVisibilityName,
        static_cast<int>(kDefaultFallbackVisibility));
    preference_manager.SetBoolean(kNearbySharingIsAnalyticsEnabledName, false);
  }

  preference_manager.SetInteger(
      kNearbySharingFastInitiationNotificationStateName,
      /*value=*/static_cast<int>(
          FastInitiationNotificationState::ENABLED_FAST_INIT));

  preference_manager.SetInteger(
      kNearbySharingDataUsageName,
      static_cast<int>(DataUsage::WIFI_ONLY_DATA_USAGE));

  preference_manager.SetString(kNearbySharingContactUploadHashName,
                               std::string());

  preference_manager.SetString(kNearbySharingDeviceIdName, std::string());

  preference_manager.SetString(kNearbySharingDeviceNameName, std::string());

  preference_manager.SetStringArray(kNearbySharingAllowedContactsName,
                                    std::vector<std::string>());

  preference_manager.SetString(kNearbySharingFullNameName, std::string());

  preference_manager.SetString(kNearbySharingIconUrlName, std::string());

  preference_manager.SetString(kNearbySharingIconTokenName, std::string());

  preference_manager.Remove(kNearbySharingPublicCertificateExpirationDictName);
  preference_manager.Remove(kNearbySharingPrivateCertificateListName);
  preference_manager.Remove(
      kNearbySharingSchedulerContactDownloadAndUploadName);
  preference_manager.Remove(kNearbySharingSchedulerDownloadDeviceDataName);
  preference_manager.Remove(
      kNearbySharingSchedulerDownloadPublicCertificatesName);
  preference_manager.Remove(
      kNearbySharingSchedulerPrivateCertificateExpirationName);
  preference_manager.Remove(
      kNearbySharingSchedulerPublicCertificateExpirationName);
  preference_manager.Remove(kNearbySharingSchedulerUploadDeviceNameName);
  preference_manager.Remove(
      kNearbySharingSchedulerUploadLocalDeviceCertificatesName);
  preference_manager.Remove(kNearbySharingUsersName);

  preference_manager.SetBoolean(kNearbySharingIsAllContactsEnabledName, true);
}

void ResetSchedulers(PreferenceManager& preference_manager) {
  preference_manager.Remove(
      kNearbySharingSchedulerContactDownloadAndUploadName);
  preference_manager.Remove(kNearbySharingSchedulerDownloadDeviceDataName);
  preference_manager.Remove(
      kNearbySharingSchedulerDownloadPublicCertificatesName);
  preference_manager.Remove(
      kNearbySharingSchedulerPrivateCertificateExpirationName);
  preference_manager.Remove(
      kNearbySharingSchedulerPublicCertificateExpirationName);
  preference_manager.Remove(kNearbySharingSchedulerUploadDeviceNameName);
  preference_manager.Remove(
      kNearbySharingSchedulerUploadLocalDeviceCertificatesName);
}

}  // namespace prefs
}  // namespace sharing
}  // namespace nearby
