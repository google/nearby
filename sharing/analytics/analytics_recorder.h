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
#include <memory>
#include <optional>
#include <string>

#include "internal/analytics/event_logger.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_device_settings.h"
#include "sharing/analytics/analytics_information.h"
#include "sharing/attachment_container.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/analytics/nearby_sharing_log.pb.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/share_target.h"

namespace nearby {
namespace sharing {
namespace analytics {

class AnalyticsRecorder {
 public:
  explicit AnalyticsRecorder(int32_t vendor_id,
                             nearby::analytics::EventLogger* event_logger)
      : vendor_id_(vendor_id), event_logger_(event_logger) {}
  ~AnalyticsRecorder() = default;

  void NewEstablishConnection(
      int64_t session_id,
      location::nearby::proto::sharing::EstablishConnectionStatus
          connection_status,
      const ShareTarget& share_target, int transfer_position,
      int concurrent_connections, int64_t duration_millis,
      std::optional<std::string> referrer_package);

  void NewAcceptAgreements();

  void NewDeclineAgreements();

  void NewAddContact();

  void NewRemoveContact();

  void NewTapFeedback();

  void NewTapHelp();

  void NewLaunchDeviceContactConsent(
      location::nearby::proto::sharing::ConsentAcceptanceStatus status);

  void NewAdvertiseDevicePresenceEnd(int64_t session_id);

  void NewAdvertiseDevicePresenceStart(
      int64_t session_id, nearby::sharing::proto::DeviceVisibility visibility,
      location::nearby::proto::sharing::SessionStatus status,
      nearby::sharing::proto::DataUsage data_usage,
      std::optional<std::string> referrer_package);

  void NewDescribeAttachments(const AttachmentContainer& attachments);

  void NewDiscoverShareTarget(
      const ShareTarget& share_target, int64_t session_id,
      int64_t latency_since_scanning_start_millis, int64_t flow_id,
      std::optional<std::string> referrer_package,
      int64_t latency_since_send_surface_registered_millis);

  void NewEnableNearbySharing(
      location::nearby::proto::sharing::NearbySharingStatus status);

  void NewOpenReceivedAttachments(const AttachmentContainer& attachments,
                                  int64_t session_id);

  void NewProcessReceivedAttachmentsEnd(
      int64_t session_id,
      location::nearby::proto::sharing::ProcessReceivedAttachmentsStatus
          status);

  void NewReceiveAttachmentsEnd(
      int64_t session_id, int64_t received_bytes,
      location::nearby::proto::sharing::AttachmentTransmissionStatus status,
      std::optional<std::string> referrer_package);

  void NewReceiveAttachmentsStart(int64_t session_id,
                                  const AttachmentContainer& attachments);

  void NewReceiveFastInitialization(int64_t timeElapseSinceScreenUnlockMillis);

  void NewAcceptFastInitialization();

  void NewDismissFastInitialization();

  void NewReceiveIntroduction(
      int64_t session_id, const ShareTarget& share_target,
      std::optional<std::string> referrer_package,
      location::nearby::proto::sharing::OSType share_target_os_type);

  void NewRespondToIntroduction(
      location::nearby::proto::sharing::ResponseToIntroduction action,
      int64_t session_id);

  void NewTapPrivacyNotification();

  void NewDismissPrivacyNotification();

  void NewScanForShareTargetsEnd(int64_t session_id);

  void NewScanForShareTargetsStart(
      int64_t session_id,
      location::nearby::proto::sharing::SessionStatus status,
      AnalyticsInformation analytics_information, int64_t flow_id,
      std::optional<std::string> referrer_package);

  void NewSendAttachmentsEnd(
      int64_t session_id, int64_t sent_bytes, const ShareTarget& share_target,
      location::nearby::proto::sharing::AttachmentTransmissionStatus status,
      int transfer_position, int concurrent_connections,
      int64_t duration_millis, std::optional<std::string> referrer_package,
      location::nearby::proto::sharing::ConnectionLayerStatus
          connection_layer_status,
      location::nearby::proto::sharing::OSType share_target_os_type);

  void NewSendAttachmentsStart(int64_t session_id,
                               const AttachmentContainer& attachments,
                               int transfer_position,
                               int concurrent_connections);

  void NewSendFastInitialization();

  void NewSendStart(int64_t session_id, int transfer_position,
                    int concurrent_connections,
                    const ShareTarget& share_target);

  void NewSendIntroduction(
      ShareTargetType target_type, int64_t session_id,
      location::nearby::proto::sharing::DeviceRelationship relationship,
      location::nearby::proto::sharing::OSType share_target_os_type);

  void NewSendIntroduction(
      int64_t session_id, const ShareTarget& share_target,
      int transfer_position, int concurrent_connections,
      location::nearby::proto::sharing::OSType share_target_os_type);

  void NewSetVisibility(nearby::sharing::proto::DeviceVisibility src_visibility,
                        nearby::sharing::proto::DeviceVisibility dst_visibility,
                        int64_t duration_millis);

  void NewDeviceSettings(AnalyticsDeviceSettings settings);

  void NewFastShareServerResponse(
      location::nearby::proto::sharing::ServerActionName name,
      location::nearby::proto::sharing::ServerResponseState state,
      int64_t latency_millis);

  void NewSetDataUsage(nearby::sharing::proto::DataUsage original_preference,
                       nearby::sharing::proto::DataUsage preference);

  void NewAddQuickSettingsTile();

  void NewRemoveQuickSettingsTile();

  void NewTapQuickSettingsTile();

  void NewToggleShowNotification(
      location::nearby::proto::sharing::ShowNotificationStatus prev_status,
      location::nearby::proto::sharing::ShowNotificationStatus current_status);

  void NewSetDeviceName(int device_name_size);

  void NewRequestSettingPermissions(
      location::nearby::proto::sharing::PermissionRequestType type,
      location::nearby::proto::sharing::PermissionRequestResult result);

  void NewInstallAPKStatus(
      location::nearby::proto::sharing::InstallAPKStatus status,
      location::nearby::proto::sharing::ApkSource source);

  void NewVerifyAPKStatus(
      location::nearby::proto::sharing::VerifyAPKStatus status,
      location::nearby::proto::sharing::ApkSource source);

  // Generates a random number for session ID or flow ID.
  int64_t GenerateNextId();

 private:
  std::unique_ptr<nearby::sharing::analytics::proto::SharingLog>
  CreateSharingLog(
      location::nearby::proto::sharing::EventCategory event_category,
      location::nearby::proto::sharing::EventType event_type);
  void LogEvent(const nearby::sharing::analytics::proto::SharingLog& message);

  const int32_t vendor_id_;
  nearby::analytics::EventLogger* event_logger_ = nullptr;
};

}  // namespace analytics
}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_ANALYTICS_ANALYTICS_RECORDER_H_
