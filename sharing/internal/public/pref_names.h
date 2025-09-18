// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_PREF_NAMES_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_PREF_NAMES_H_

#include "absl/strings/string_view.h"

namespace nearby::sharing {

// A class container for Nearby Share prefs names.
class PrefNames {
 public:
  static constexpr absl::string_view kVisibility =
      "nearby_sharing.background_visibility";
  static constexpr absl::string_view kFallbackVisibility =
      "nearby_sharing.background_fallback_visibility";
  static constexpr absl::string_view kVisibilityExpirationSeconds =
      "nearby_sharing.background_visibility_expiration_seconds";
  static constexpr absl::string_view kCustomSavePath =
      "nearby_sharing.custom_save_path";
  static constexpr absl::string_view kDataUsage = "nearby_sharing.data_usage";
  static constexpr absl::string_view kDeviceId = "nearby_sharing.device_id";
  static constexpr absl::string_view kDeviceName = "nearby_sharing.device_name";
  static constexpr absl::string_view kFastInitiationNotificationState =
      "nearby_sharing.fast_initiation_notification_state";
  static constexpr absl::string_view kPrivateCertificateList =
      "nearby_sharing.private_certificate_list";
  static constexpr absl::string_view kPublicCertificateExpirationDict =
      "nearbyshare.public_certificate_expiration_dict";
  static constexpr absl::string_view kSchedulerDownloadPublicCertificates =
      "nearby_sharing.scheduler.download_public_certificates";
  static constexpr absl::string_view kSchedulerPrivateCertificateExpiration =
      "nearby_sharing.scheduler.private_certificate_expiration";
  static constexpr absl::string_view kSchedulerPublicCertificateExpiration =
      "nearby_sharing.scheduler.public_certificate_expiration";
  static constexpr absl::string_view kSchedulerUploadLocalDeviceCertificates =
      "nearby_sharing.scheduler.upload_local_device_certificates";
  static constexpr absl::string_view kUsers = "nearby_sharing.users";
  static constexpr absl::string_view kIsAnalyticsEnabled =
      "nearby_sharing.is_analytics_enabled";
  static constexpr absl::string_view kAdvancedProtectionEnabled =
      "nearby_sharing.advanced_protection_enabled";
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_PREF_NAMES_H_
