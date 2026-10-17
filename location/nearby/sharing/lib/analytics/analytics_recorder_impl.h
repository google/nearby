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

#ifndef LOCATION_NEARBY_SHARING_LIB_ANALYTICS_ANALYTICS_RECORDER_IMPL_H_
#define LOCATION_NEARBY_SHARING_LIB_ANALYTICS_ANALYTICS_RECORDER_IMPL_H_

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "location/nearby/analytics/cpp/logging/event_logger.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_device_settings.h"
#include "sharing/analytics/analytics_information.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment_container.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/share_target.h"

namespace nearby::sharing::analytics {

// Sharing analytics recorder. Google's implementation builds SharingLog
// events for its analytics backend; this build records nothing and only
// hands out the session ids the service requires.
class AnalyticsRecorderImpl : public AnalyticsRecorder {
 public:
  // The event logger may be null.
  AnalyticsRecorderImpl(int32_t vendor_id,
                        nearby::analytics::EventLogger* event_logger) {}

  void NewEstablishConnection( int64_t session_id, location::nearby::proto::sharing::EstablishConnectionStatus connection_status, const ShareTarget& share_target, int transfer_position, int concurrent_connections, int64_t duration_millis, std::optional<std::string> referrer_package) override {}
  void NewAcceptAgreements() override {}
  void NewDeclineAgreements() override {}
  void NewAddContact() override {}
  void NewRemoveContact() override {}
  void NewTapFeedback() override {}
  void NewTapHelp() override {}
  void NewLaunchDeviceContactConsent( location::nearby::proto::sharing::ConsentAcceptanceStatus status) override {}
  void NewAdvertiseDevicePresenceEnd(int64_t session_id) override {}
  void NewAdvertiseDevicePresenceStart( int64_t session_id, nearby::sharing::proto::DeviceVisibility visibility, location::nearby::proto::sharing::SessionStatus status, nearby::sharing::proto::DataUsage data_usage, std::optional<std::string> referrer_package) override {}
  void NewDescribeAttachments( const AttachmentContainer& attachments) override {}
  void NewDiscoverShareTarget( const ShareTarget& share_target, int64_t session_id, int64_t latency_since_scanning_start_millis, int64_t flow_id, std::optional<std::string> referrer_package, int64_t latency_since_send_surface_registered_millis) override {}
  void NewEnableNearbySharing( location::nearby::proto::sharing::NearbySharingStatus status) override {}
  void NewOpenReceivedAttachments( const AttachmentContainer& attachments, int64_t session_id) override {}
  void NewProcessReceivedAttachmentsEnd( int64_t session_id, location::nearby::proto::sharing::ProcessReceivedAttachmentsStatus status) override {}
  void NewReceiveAttachmentsEnd( int64_t session_id, int64_t received_bytes, location::nearby::proto::sharing::AttachmentTransmissionStatus status, std::optional<std::string> referrer_package) override {}
  void NewReceiveAttachmentsStart( int64_t session_id, const AttachmentContainer& attachments) override {}
  void NewReceiveFastInitialization( int64_t timeElapseSinceScreenUnlockMillis) override {}
  void NewAcceptFastInitialization() override {}
  void NewDismissFastInitialization() override {}
  void NewReceiveIntroduction( int64_t session_id, const ShareTarget& share_target, std::optional<std::string> referrer_package, location::nearby::proto::sharing::OSType share_target_os_type, location::nearby::proto::sharing::SharingUseCase sharing_use_case, location::nearby::proto::sharing::PowerStatus power_status) override {}
  void NewRespondToIntroduction( location::nearby::proto::sharing::ResponseToIntroduction action, int64_t session_id) override {}
  void NewTapPrivacyNotification() override {}
  void NewDismissPrivacyNotification() override {}
  void NewScanForShareTargetsEnd(int64_t session_id) override {}
  void NewScanForShareTargetsStart( int64_t session_id, location::nearby::proto::sharing::SessionStatus status, AnalyticsInformation analytics_information, int64_t flow_id, std::optional<std::string> referrer_package) override {}
  void NewSendAttachmentsEnd( int64_t session_id, int64_t sent_bytes, const ShareTarget& share_target, location::nearby::proto::sharing::AttachmentTransmissionStatus status, int transfer_position, int concurrent_connections, int64_t duration_millis, std::optional<std::string> referrer_package, location::nearby::proto::sharing::ConnectionLayerStatus connection_layer_status, location::nearby::proto::sharing::OSType share_target_os_type) override {}
  void NewSendAttachmentsStart(int64_t session_id, const AttachmentContainer& attachments, int transfer_position, int concurrent_connections, bool advanced_protection_enabled) override {}
  void NewSendFastInitialization() override {}
  void NewSendStart(int64_t session_id, int transfer_position, int concurrent_connections, const ShareTarget& share_target) override {}
  void NewSendIntroduction( ShareTargetType target_type, int64_t session_id, location::nearby::proto::sharing::DeviceRelationship relationship, location::nearby::proto::sharing::OSType share_target_os_type) override {}
  void NewSendIntroduction( int64_t session_id, const ShareTarget& share_target, int transfer_position, int concurrent_connections, location::nearby::proto::sharing::OSType share_target_os_type, location::nearby::proto::sharing::SharingUseCase sharing_use_case, location::nearby::proto::sharing::PowerStatus power_status) override {}
  void NewSetVisibility( nearby::sharing::proto::DeviceVisibility src_visibility, nearby::sharing::proto::DeviceVisibility dst_visibility, int64_t duration_millis) override {}
  void NewDeviceSettings(AnalyticsDeviceSettings settings) override {}
  void NewSetDataUsage( nearby::sharing::proto::DataUsage original_preference, nearby::sharing::proto::DataUsage preference) override {}
  void NewAddQuickSettingsTile() override {}
  void NewRemoveQuickSettingsTile() override {}
  void NewTapQuickSettingsTile() override {}
  void NewToggleShowNotification( location::nearby::proto::sharing::ShowNotificationStatus prev_status, location::nearby::proto::sharing::ShowNotificationStatus current_status) override {}
  void NewSetDeviceName(int device_name_size) override {}
  void NewRequestSettingPermissions( location::nearby::proto::sharing::PermissionRequestType type, location::nearby::proto::sharing::PermissionRequestResult result) override {}
  void NewInstallAPKStatus( location::nearby::proto::sharing::InstallAPKStatus status, location::nearby::proto::sharing::ApkSource source) override {}
  void NewVerifyAPKStatus( location::nearby::proto::sharing::VerifyAPKStatus status, location::nearby::proto::sharing::ApkSource source) override {}
  void NewRpcCallStatus(absl::string_view rpc_name, RpcDirection direction, int error_code, absl::Duration latency) override {}
  void NewPairDevicesStatus( location::nearby::proto::sharing::PairingResultStatus status, absl::Duration duration) override {}
  void NewUnpairDevice( location::nearby::proto::sharing::UnpairTriggerType trigger_type) override {}

  int64_t GenerateNextId() override { return ++next_id_; }

 private:
  std::atomic<int64_t> next_id_ = 0;
};

}  // namespace nearby::sharing::analytics

#endif  // LOCATION_NEARBY_SHARING_LIB_ANALYTICS_ANALYTICS_RECORDER_IMPL_H_
