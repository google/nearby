// Copyright 2022-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_ANALYTICS_ANALYTICS_RECORDER_H_
#define THIRD_PARTY_NEARBY_SHARING_ANALYTICS_ANALYTICS_RECORDER_H_

#include <cstdint>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_device_settings.h"
#include "sharing/analytics/analytics_information.h"
#include "sharing/attachment_container.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/share_target.h"

namespace nearby::sharing::analytics {

class AnalyticsRecorder {
 public:
  enum class RpcDirection {
    kUnknown = 0,
    kIncoming = 1,
    kOutgoing = 2,
  };

  AnalyticsRecorder() = default;
  virtual ~AnalyticsRecorder() = default;

  virtual void NewEstablishConnection(
      int64_t session_id,
      location::nearby::proto::sharing::EstablishConnectionStatus
          connection_status,
      const ShareTarget& share_target, int transfer_position,
      int concurrent_connections, int64_t duration_millis,
      std::optional<std::string> referrer_package) = 0;

  virtual void NewAcceptAgreements() = 0;

  virtual void NewDeclineAgreements() = 0;

  virtual void NewAddContact() = 0;

  virtual void NewRemoveContact() = 0;

  virtual void NewTapFeedback() = 0;

  virtual void NewTapHelp() = 0;

  virtual void NewLaunchDeviceContactConsent(
      location::nearby::proto::sharing::ConsentAcceptanceStatus status) = 0;

  virtual void NewAdvertiseDevicePresenceEnd(int64_t session_id) = 0;

  virtual void NewAdvertiseDevicePresenceStart(
      int64_t session_id, nearby::sharing::proto::DeviceVisibility visibility,
      location::nearby::proto::sharing::SessionStatus status,
      nearby::sharing::proto::DataUsage data_usage,
      std::optional<std::string> referrer_package) = 0;

  virtual void NewDescribeAttachments(
      const AttachmentContainer& attachments) = 0;

  virtual void NewDiscoverShareTarget(
      const ShareTarget& share_target, int64_t session_id,
      int64_t latency_since_scanning_start_millis, int64_t flow_id,
      std::optional<std::string> referrer_package,
      int64_t latency_since_send_surface_registered_millis) = 0;

  virtual void NewEnableNearbySharing(
      location::nearby::proto::sharing::NearbySharingStatus status) = 0;

  virtual void NewOpenReceivedAttachments(
      const AttachmentContainer& attachments, int64_t session_id) = 0;

  virtual void NewProcessReceivedAttachmentsEnd(
      int64_t session_id,
      location::nearby::proto::sharing::ProcessReceivedAttachmentsStatus
          status) = 0;

  virtual void NewReceiveAttachmentsEnd(
      int64_t session_id, int64_t received_bytes,
      location::nearby::proto::sharing::AttachmentTransmissionStatus status,
      std::optional<std::string> referrer_package) = 0;

  virtual void NewReceiveAttachmentsStart(
      int64_t session_id, const AttachmentContainer& attachments) = 0;

  virtual void NewReceiveFastInitialization(
      int64_t timeElapseSinceScreenUnlockMillis) = 0;

  virtual void NewAcceptFastInitialization() = 0;

  virtual void NewDismissFastInitialization() = 0;

  virtual void NewReceiveIntroduction(
      int64_t session_id, const ShareTarget& share_target,
      std::optional<std::string> referrer_package,
      location::nearby::proto::sharing::OSType share_target_os_type) = 0;

  virtual void NewRespondToIntroduction(
      location::nearby::proto::sharing::ResponseToIntroduction action,
      int64_t session_id) = 0;

  virtual void NewTapPrivacyNotification() = 0;

  virtual void NewDismissPrivacyNotification() = 0;

  virtual void NewScanForShareTargetsEnd(int64_t session_id) = 0;

  virtual void NewScanForShareTargetsStart(
      int64_t session_id,
      location::nearby::proto::sharing::SessionStatus status,
      AnalyticsInformation analytics_information, int64_t flow_id,
      std::optional<std::string> referrer_package) = 0;

  virtual void NewSendAttachmentsEnd(
      int64_t session_id, int64_t sent_bytes, const ShareTarget& share_target,
      location::nearby::proto::sharing::AttachmentTransmissionStatus status,
      int transfer_position, int concurrent_connections,
      int64_t duration_millis, std::optional<std::string> referrer_package,
      location::nearby::proto::sharing::ConnectionLayerStatus
          connection_layer_status,
      location::nearby::proto::sharing::OSType share_target_os_type) = 0;

  virtual void NewSendAttachmentsStart(int64_t session_id,
                                       const AttachmentContainer& attachments,
                                       int transfer_position,
                                       int concurrent_connections,
                                       bool advanced_protection_enabled,
                                       bool advanced_protection_mismatch) = 0;

  virtual void NewSendFastInitialization() = 0;

  virtual void NewSendStart(int64_t session_id, int transfer_position,
                            int concurrent_connections,
                            const ShareTarget& share_target) = 0;

  virtual void NewSendIntroduction(
      ShareTargetType target_type, int64_t session_id,
      location::nearby::proto::sharing::DeviceRelationship relationship,
      location::nearby::proto::sharing::OSType share_target_os_type) = 0;

  virtual void NewSendIntroduction(
      int64_t session_id, const ShareTarget& share_target,
      int transfer_position, int concurrent_connections,
      location::nearby::proto::sharing::OSType share_target_os_type) = 0;

  virtual void NewSetVisibility(
      nearby::sharing::proto::DeviceVisibility src_visibility,
      nearby::sharing::proto::DeviceVisibility dst_visibility,
      int64_t duration_millis) = 0;

  virtual void NewDeviceSettings(AnalyticsDeviceSettings settings) = 0;

  virtual void NewSetDataUsage(
      nearby::sharing::proto::DataUsage original_preference,
      nearby::sharing::proto::DataUsage preference) = 0;

  virtual void NewAddQuickSettingsTile() = 0;

  virtual void NewRemoveQuickSettingsTile() = 0;

  virtual void NewTapQuickSettingsTile() = 0;

  virtual void NewToggleShowNotification(
      location::nearby::proto::sharing::ShowNotificationStatus prev_status,
      location::nearby::proto::sharing::ShowNotificationStatus
          current_status) = 0;

  virtual void NewSetDeviceName(int device_name_size) = 0;

  virtual void NewRequestSettingPermissions(
      location::nearby::proto::sharing::PermissionRequestType type,
      location::nearby::proto::sharing::PermissionRequestResult result) = 0;

  virtual void NewInstallAPKStatus(
      location::nearby::proto::sharing::InstallAPKStatus status,
      location::nearby::proto::sharing::ApkSource source) = 0;

  virtual void NewVerifyAPKStatus(
      location::nearby::proto::sharing::VerifyAPKStatus status,
      location::nearby::proto::sharing::ApkSource source) = 0;

  virtual void NewRpcCallStatus(absl::string_view rpc_name,
                                RpcDirection direction, int error_code,
                                absl::Duration latency) = 0;

  // Generates a random number for session ID or flow ID.
  virtual int64_t GenerateNextId() = 0;
};

}  // namespace nearby::sharing::analytics

#endif  // THIRD_PARTY_NEARBY_SHARING_ANALYTICS_ANALYTICS_RECORDER_H_
