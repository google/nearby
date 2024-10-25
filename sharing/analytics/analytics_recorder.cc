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

#include "sharing/analytics/analytics_recorder.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "google/protobuf/duration.pb.h"
#include "absl/random/random.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_device_settings.h"
#include "sharing/analytics/analytics_information.h"
#include "sharing/attachment.h"
#include "sharing/attachment_container.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/file_attachment.h"
#include "sharing/proto/analytics/nearby_sharing_log.pb.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/share_target.h"
#include "sharing/wifi_credentials_attachment.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace nearby {
namespace sharing {
namespace analytics {
namespace {

using ::location::nearby::proto::sharing::AttachmentSourceType;
using ::location::nearby::proto::sharing::DeviceRelationship;
using ::location::nearby::proto::sharing::DeviceType;
using ::location::nearby::proto::sharing::EstablishConnectionStatus;
using ::location::nearby::proto::sharing::EventCategory;
using ::location::nearby::proto::sharing::EventType;
using ::location::nearby::proto::sharing::OSType;
using ::location::nearby::proto::sharing::ProcessReceivedAttachmentsStatus;
using ::location::nearby::proto::sharing::ShowNotificationStatus;
using ::location::nearby::proto::sharing::Visibility;

using ::nearby::sharing::analytics::proto::SharingLog;
using ::nearby::sharing::proto::DataUsage;
using ::nearby::sharing::proto::DeviceVisibility;

DeviceRelationship GetLoggerDeviceRelationship(
    const ShareTarget& share_target) {
  if (share_target.for_self_share) {
    return DeviceRelationship::IS_SELF;
  } else if (share_target.is_known) {
    return DeviceRelationship::IS_CONTACT;
  } else {
    return DeviceRelationship::IS_STRANGER;
  }
}

DeviceType GetLoggerDeviceType(ShareTargetType type) {
  switch (type) {
    case ShareTargetType::kLaptop:
      return DeviceType::LAPTOP;
    case ShareTargetType::kPhone:
      return DeviceType::PHONE;
    case ShareTargetType::kTablet:
      return DeviceType::TABLET;
    default:
      return DeviceType::UNKNOWN_DEVICE_TYPE;
  }
}

Visibility GetLoggerVisibility(DeviceVisibility visibility) {
  switch (visibility) {
    case DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS:
      return Visibility::CONTACTS_ONLY;
    case DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS:
      return Visibility::SELECTED_CONTACTS_ONLY;
    case DeviceVisibility::DEVICE_VISIBILITY_EVERYONE:
      return Visibility::EVERYONE;
    case DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE:
      return Visibility::SELF_SHARE;
    case DeviceVisibility::DEVICE_VISIBILITY_HIDDEN:
      return Visibility::HIDDEN;
    case DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED:
    default:
      return Visibility::UNKNOWN_VISIBILITY;
  }
}

location::nearby::proto::sharing::DataUsage GetLoggerDataUsage(
    DataUsage data_usage) {
  switch (data_usage) {
    case DataUsage::OFFLINE_DATA_USAGE:
      return location::nearby::proto::sharing::DataUsage::OFFLINE;
    case DataUsage::ONLINE_DATA_USAGE:
      return location::nearby::proto::sharing::DataUsage::ONLINE;
    case DataUsage::WIFI_ONLY_DATA_USAGE:
      return location::nearby::proto::sharing::DataUsage::WIFI_ONLY;
    default:
      return location::nearby::proto::sharing::DataUsage::UNKNOWN_DATA_USAGE;
  }
}

AttachmentSourceType GetLoggerAttachmentSourceType(
    Attachment::SourceType source_type) {
  switch (source_type) {
    case Attachment::SourceType::kContextMenu:
      return AttachmentSourceType::ATTACHMENT_SOURCE_CONTEXT_MENU;
    case Attachment::SourceType::kDragAndDrop:
      return AttachmentSourceType::ATTACHMENT_SOURCE_DRAG_AND_DROP;
    case Attachment::SourceType::kSelectFilesButton:
      return AttachmentSourceType::ATTACHMENT_SOURCE_SELECT_FILES_BUTTON;
    case Attachment::SourceType::kPaste:
      return AttachmentSourceType::ATTACHMENT_SOURCE_PASTE;
    case Attachment::SourceType::kSelectFoldersButton:
      return AttachmentSourceType::ATTACHMENT_SOURCE_SELECT_FOLDERS_BUTTON;
    default:
      return AttachmentSourceType::ATTACHMENT_SOURCE_UNKNOWN;
  }
}

void SetShareTargetInfo(SharingLog::ShareTargetInfo* share_target_info,
                        ShareTargetType device_type,
                        DeviceRelationship relationship,
                        OSType os_type = OSType::UNKNOWN_OS_TYPE) {
  share_target_info->set_device_relationship(relationship);
  share_target_info->set_device_type(GetLoggerDeviceType(device_type));
  if (os_type == OSType::UNKNOWN_OS_TYPE &&
      device_type == ShareTargetType::kPhone) {
    // If the device type is phone, just set the OS type to android because
    // no other phone OS for now.
    share_target_info->set_os_type(OSType::ANDROID);
  } else {
    share_target_info->set_os_type(os_type);
  }
}

void SetShareTargetInfo(SharingLog::ShareTargetInfo* share_target_info,
                        const ShareTarget& share_target,
                        OSType os_type = OSType::UNKNOWN_OS_TYPE) {
  share_target_info->set_device_relationship(
      GetLoggerDeviceRelationship(share_target));
  share_target_info->set_device_type(GetLoggerDeviceType(share_target.type));
  if (os_type == OSType::UNKNOWN_OS_TYPE &&
      share_target.type == ShareTargetType::kPhone) {
    // If the device type is phone, just set the OS type to android because
    // no other phone OS for now.
    share_target_info->set_os_type(OSType::ANDROID);
  } else {
    share_target_info->set_os_type(os_type);
  }
}

void SetAttachmentInfo(SharingLog::AttachmentsInfo* attachments_info,
                       const AttachmentContainer& attachments) {
  for (const auto& attachment : attachments.GetTextAttachments()) {
    SharingLog::TextAttachment::Type type =
        SharingLog::TextAttachment::UNKNOWN_TEXT_TYPE;
    switch (attachment.GetShareType()) {
      case ShareType::kPhone:
        type = SharingLog::TextAttachment::PHONE_NUMBER;
        break;
      case ShareType::kUrl:
        type = SharingLog::TextAttachment::URL;
        break;
      case ShareType::kAddress:
        type = SharingLog::TextAttachment::ADDRESS;
        break;
      case ShareType::kText:
        // Apply UNKNOWN_TEXT_TYPE for it based on analytics design.
        break;
      default:
        break;
    }
    SharingLog::TextAttachment* text_attachment =
        attachments_info->mutable_text_attachment()->Add();
    text_attachment->set_type(type);
    text_attachment->set_size_bytes(attachment.size());
    text_attachment->set_source_type(
        GetLoggerAttachmentSourceType(attachment.source_type()));
    text_attachment->set_batch_id(attachment.batch_id());
  }

  for (const auto& attachment : attachments.GetFileAttachments()) {
    SharingLog::FileAttachment::Type type =
        SharingLog::FileAttachment::UNKNOWN_FILE_TYPE;
    switch (attachment.GetShareType()) {
      case ShareType::kImageFile:
        type = SharingLog::FileAttachment::IMAGE;
        break;
      case ShareType::kVideoFile:
        type = SharingLog::FileAttachment::VIDEO;
        break;
      case ShareType::kAudioFile:
        type = SharingLog::FileAttachment::AUDIO;
        break;
      case ShareType::kPdfFile:
      case ShareType::kTextFile:
      case ShareType::kGoogleDocsFile:
      case ShareType::kGoogleSheetsFile:
      case ShareType::kGoogleSlidesFile:
        type = SharingLog::FileAttachment::DOCUMENT;
        break;
      case ShareType::kUnknownFile:
        // The default type is set to type.
        break;
      default:
        break;
    }
    SharingLog::FileAttachment* file_attachment =
        attachments_info->mutable_file_attachment()->Add();
    file_attachment->set_type(type);
    file_attachment->set_size_bytes(attachment.size());
    file_attachment->set_offset_bytes(0);
    file_attachment->set_source_type(
        GetLoggerAttachmentSourceType(attachment.source_type()));
    file_attachment->set_batch_id(attachment.batch_id());
  }

  for (const auto& attachment : attachments.GetWifiCredentialsAttachments()) {
    SharingLog::WifiCredentialsAttachment* wifi_credentials_attachment =
        attachments_info->mutable_wifi_credentials_attachment()->Add();
    wifi_credentials_attachment->set_source_type(
        GetLoggerAttachmentSourceType(attachment.source_type()));
    wifi_credentials_attachment->set_batch_id(attachment.batch_id());
  }
}

}  // namespace

void AnalyticsRecorder::NewEstablishConnection(
    int64_t session_id, EstablishConnectionStatus connection_status,
    const ShareTarget& share_target, int transfer_position,
    int concurrent_connections, int64_t duration_millis,
    std::optional<std::string> referrer_package) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::ESTABLISH_CONNECTION);

  auto* establish_connection = sharing_log->mutable_establish_connection();

  establish_connection->set_session_id(session_id);
  establish_connection->set_status(connection_status);
  SetShareTargetInfo(establish_connection->mutable_share_target_info(),
                     share_target);
  establish_connection->set_transfer_position(transfer_position);
  establish_connection->set_concurrent_connections(concurrent_connections);
  establish_connection->set_duration_millis(duration_millis);
  if (referrer_package.has_value()) {
    establish_connection->set_referrer_name(*referrer_package);
  }

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewAcceptAgreements() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::ACCEPT_AGREEMENTS);

  sharing_log->mutable_accept_agreements();

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewDeclineAgreements() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::DECLINE_AGREEMENTS);

  sharing_log->mutable_decline_agreements();
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewAddContact() {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::SETTINGS_EVENT, EventType::ADD_CONTACT);

  sharing_log->mutable_add_contact();
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewRemoveContact() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::REMOVE_CONTACT);

  sharing_log->mutable_remove_contact();
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewTapFeedback() {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::SETTINGS_EVENT, EventType::TAP_FEEDBACK);

  sharing_log->mutable_tap_feedback();
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewTapHelp() {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::SETTINGS_EVENT, EventType::TAP_HELP);

  sharing_log->mutable_tap_help();
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewLaunchDeviceContactConsent(
    ::location::nearby::proto::sharing::ConsentAcceptanceStatus status) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::LAUNCH_CONSENT);

  auto* launch_consent = sharing_log->mutable_launch_consent();
  launch_consent->set_status(status);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewAdvertiseDevicePresenceEnd(int64_t session_id) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::ADVERTISE_DEVICE_PRESENCE_END);

  auto* advertise_device_presence_end =
      sharing_log->mutable_advertise_device_presence_end();
  advertise_device_presence_end->set_session_id(session_id);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewAdvertiseDevicePresenceStart(
    int64_t session_id, DeviceVisibility visibility,
    ::location::nearby::proto::sharing::SessionStatus status,
    DataUsage data_usage, std::optional<std::string> referrer_package) {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::RECEIVING_EVENT,
                       EventType::ADVERTISE_DEVICE_PRESENCE_START);

  auto* advertise_device_presence_start =
      sharing_log->mutable_advertise_device_presence_start();
  advertise_device_presence_start->set_session_id(session_id);
  advertise_device_presence_start->set_visibility(
      GetLoggerVisibility(visibility));
  advertise_device_presence_start->set_status(status);
  advertise_device_presence_start->set_data_usage(
      GetLoggerDataUsage(data_usage));
  if (referrer_package.has_value()) {
    advertise_device_presence_start->set_referrer_name(*referrer_package);
  }

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewDescribeAttachments(
    const AttachmentContainer& attachments) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::DESCRIBE_ATTACHMENTS);

  auto* describe_attachments = sharing_log->mutable_describe_attachments();
  SetAttachmentInfo(describe_attachments->mutable_attachments_info(),
                    attachments);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewDiscoverShareTarget(
    const ShareTarget& share_target, int64_t session_id,
    int64_t latency_since_scanning_start_millis, int64_t flow_id,
    std::optional<std::string> referrer_package,
    int64_t latency_since_send_surface_registered_millis) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::DISCOVER_SHARE_TARGET);

  auto* discover_share_target = sharing_log->mutable_discover_share_target();
  discover_share_target->set_session_id(session_id);
  auto* duration = discover_share_target->mutable_duration_since_scanning();
  duration->set_seconds(latency_since_scanning_start_millis / 1000);
  duration->set_nanos((latency_since_scanning_start_millis % 1000) * 1000000);
  SetShareTargetInfo(discover_share_target->mutable_share_target_info(),
                     share_target);
  discover_share_target->set_session_id(session_id);
  discover_share_target->set_flow_id(flow_id);

  discover_share_target->set_latency_since_activity_start_millis(
      latency_since_send_surface_registered_millis > 0
          ? latency_since_send_surface_registered_millis
          : -1);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewEnableNearbySharing(
    ::location::nearby::proto::sharing::NearbySharingStatus status) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::ENABLE_NEARBY_SHARING);

  auto* enable_nearby_sharing = sharing_log->mutable_enable_nearby_sharing();
  enable_nearby_sharing->set_status(status);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewOpenReceivedAttachments(
    const AttachmentContainer& attachments, int64_t session_id) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::OPEN_RECEIVED_ATTACHMENTS);

  auto* open_received_attachments =
      sharing_log->mutable_open_received_attachments();
  SetAttachmentInfo(open_received_attachments->mutable_attachments_info(),
                    attachments);
  open_received_attachments->set_session_id(session_id);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewProcessReceivedAttachmentsEnd(
    int64_t session_id, ProcessReceivedAttachmentsStatus status) {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::RECEIVING_EVENT,
                       EventType::PROCESS_RECEIVED_ATTACHMENTS_END);

  auto* process_received_attachments_end =
      sharing_log->mutable_process_received_attachments_end();
  process_received_attachments_end->set_status(status);
  process_received_attachments_end->set_session_id(session_id);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewReceiveAttachmentsEnd(
    int64_t session_id, int64_t received_bytes,
    ::location::nearby::proto::sharing::AttachmentTransmissionStatus status,
    std::optional<std::string> referrer_package) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::RECEIVE_ATTACHMENTS_END);

  auto* receive_attachments_end =
      sharing_log->mutable_receive_attachments_end();
  receive_attachments_end->set_session_id(session_id);
  receive_attachments_end->set_received_bytes(received_bytes);
  receive_attachments_end->set_status(status);
  if (referrer_package.has_value()) {
    receive_attachments_end->set_referrer_name(*referrer_package);
  }

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewReceiveAttachmentsStart(
    int64_t session_id, const AttachmentContainer& attachments) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::RECEIVE_ATTACHMENTS_START);

  auto* receive_attachments_start =
      sharing_log->mutable_receive_attachments_start();
  SetAttachmentInfo(receive_attachments_start->mutable_attachments_info(),
                    attachments);
  receive_attachments_start->set_session_id(session_id);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewReceiveFastInitialization(
    int64_t timeElapseSinceScreenUnlockMillis) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::RECEIVE_FAST_INITIALIZATION);

  auto* receive_fast_initialization =
      sharing_log->mutable_receive_initialization();

  receive_fast_initialization->set_time_elapse_since_screen_unlock_millis(
      timeElapseSinceScreenUnlockMillis);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewAcceptFastInitialization() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::ACCEPT_FAST_INITIALIZATION);

  sharing_log->mutable_accept_fast_initialization();

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewDismissFastInitialization() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::DISMISS_FAST_INITIALIZATION);

  sharing_log->mutable_dismiss_fast_initialization();

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewReceiveIntroduction(
    int64_t session_id, const ShareTarget& share_target,
    std::optional<std::string> referrer_package,
    ::location::nearby::proto::sharing::OSType share_target_os_type) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::RECEIVE_INTRODUCTION);

  auto* receive_introduction = sharing_log->mutable_receive_introduction();
  receive_introduction->set_session_id(session_id);
  SetShareTargetInfo(receive_introduction->mutable_share_target_info(),
                     share_target, share_target_os_type);
  if (referrer_package.has_value()) {
    receive_introduction->set_referrer_name(*referrer_package);
  }

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewRespondToIntroduction(
    ::location::nearby::proto::sharing::ResponseToIntroduction action,
    int64_t session_id) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::RESPOND_TO_INTRODUCTION);

  auto* respond_to_introduction = sharing_log->mutable_respond_introduction();
  respond_to_introduction->set_session_id(session_id);
  respond_to_introduction->set_action(action);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewTapPrivacyNotification() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::TAP_PRIVACY_NOTIFICATION);

  sharing_log->mutable_tap_privacy_notification();

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewDismissPrivacyNotification() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::DISMISS_PRIVACY_NOTIFICATION);

  sharing_log->mutable_dismiss_privacy_notification();

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewScanForShareTargetsEnd(int64_t session_id) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SCAN_FOR_SHARE_TARGETS_END);

  auto* scan_for_share_targets_end =
      sharing_log->mutable_scan_for_share_targets_end();
  scan_for_share_targets_end->set_session_id(session_id);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewScanForShareTargetsStart(
    int64_t session_id,
    ::location::nearby::proto::sharing::SessionStatus status,
    AnalyticsInformation analytics_information, int64_t flow_id,
    std::optional<std::string> referrer_package) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SCAN_FOR_SHARE_TARGETS_START);

  auto* scan_for_share_targets_start =
      sharing_log->mutable_scan_for_share_targets_start();
  scan_for_share_targets_start->set_session_id(session_id);
  scan_for_share_targets_start->set_status(status);
  scan_for_share_targets_start->set_scan_type(
      static_cast<::location::nearby::proto::sharing::ScanType>(
          analytics_information.send_surface_state));
  scan_for_share_targets_start->set_flow_id(flow_id);
  if (referrer_package.has_value()) {
    scan_for_share_targets_start->set_referrer_name(*referrer_package);
  }

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendAttachmentsEnd(
    int64_t session_id, int64_t sent_bytes, const ShareTarget& share_target,
    ::location::nearby::proto::sharing::AttachmentTransmissionStatus status,
    int transfer_position, int concurrent_connections, int64_t duration_millis,
    std::optional<std::string> referrer_package,
    ::location::nearby::proto::sharing::ConnectionLayerStatus
        connection_layer_status,
    ::location::nearby::proto::sharing::OSType share_target_os_type) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SEND_ATTACHMENTS_END);

  auto* send_attachments_end = sharing_log->mutable_send_attachments_end();
  send_attachments_end->set_session_id(session_id);
  send_attachments_end->set_sent_bytes(sent_bytes);
  SetShareTargetInfo(send_attachments_end->mutable_share_target_info(),
                     share_target, share_target_os_type);
  send_attachments_end->set_status(status);
  send_attachments_end->set_transfer_position(transfer_position);
  send_attachments_end->set_concurrent_connections(concurrent_connections);
  send_attachments_end->set_duration_millis(duration_millis);
  if (referrer_package.has_value()) {
    send_attachments_end->set_referrer_name(*referrer_package);
  }
  send_attachments_end->set_connection_layer_status(connection_layer_status);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendAttachmentsStart(
    int64_t session_id, const AttachmentContainer& attachments,
    int transfer_position, int concurrent_connections) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SEND_ATTACHMENTS_START);

  auto* send_attachments_start = sharing_log->mutable_send_attachments_start();
  send_attachments_start->set_session_id(session_id);
  SetAttachmentInfo(send_attachments_start->mutable_attachments_info(),
                    attachments);
  send_attachments_start->set_transfer_position(transfer_position);
  send_attachments_start->set_concurrent_connections(concurrent_connections);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendFastInitialization() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SEND_FAST_INITIALIZATION);

  sharing_log->mutable_send_initialization();

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendStart(int64_t session_id, int transfer_position,
                                     int concurrent_connections,
                                     const ShareTarget& share_target) {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::SENDING_EVENT, EventType::SEND_START);

  auto* send_start = sharing_log->mutable_send_start();
  send_start->set_session_id(session_id);
  send_start->set_transfer_position(transfer_position);
  send_start->set_concurrent_connections(concurrent_connections);
  SetShareTargetInfo(send_start->mutable_share_target_info(), share_target);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendIntroduction(
    ShareTargetType target_type, int64_t session_id,
    DeviceRelationship relationship,
    ::location::nearby::proto::sharing::OSType share_target_os_type) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SEND_INTRODUCTION);
  auto* send_introduction = sharing_log->mutable_send_introduction();
  SetShareTargetInfo(send_introduction->mutable_share_target_info(),
                     target_type, relationship, share_target_os_type);
  send_introduction->set_session_id(session_id);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendIntroduction(
    int64_t session_id, const ShareTarget& share_target, int transfer_position,
    int concurrent_connections,
    ::location::nearby::proto::sharing::OSType share_target_os_type) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SEND_INTRODUCTION);

  auto* send_introduction = sharing_log->mutable_send_introduction();
  SetShareTargetInfo(send_introduction->mutable_share_target_info(),
                     share_target, share_target_os_type);
  send_introduction->set_session_id(session_id);
  send_introduction->set_transfer_position(transfer_position);
  send_introduction->set_concurrent_connections(concurrent_connections);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSetVisibility(DeviceVisibility src_visibility,
                                         DeviceVisibility dst_visibility,
                                         int64_t duration_millis) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::SET_VISIBILITY);

  auto* set_visibility = sharing_log->mutable_set_visibility();
  set_visibility->set_visibility(GetLoggerVisibility(dst_visibility));
  set_visibility->set_source_visibility(GetLoggerVisibility(src_visibility));
  set_visibility->set_duration_millis(duration_millis);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewDeviceSettings(AnalyticsDeviceSettings settings) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::DEVICE_SETTINGS);

  auto* device_settings = sharing_log->mutable_device_settings();
  device_settings->set_data_usage(GetLoggerDataUsage(settings.data_usage));
  device_settings->set_device_name_size(settings.device_name_size);
  device_settings->set_is_show_notification_enabled(
      settings.is_fast_init_notification_enabled);
  device_settings->set_visibility(GetLoggerVisibility(settings.visibility));

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewFastShareServerResponse(
    ::location::nearby::proto::sharing::ServerActionName name,
    ::location::nearby::proto::sharing::ServerResponseState state,
    int64_t latency_millis) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::FAST_SHARE_SERVER_RESPONSE);

  auto* fast_share_server_response =
      sharing_log->mutable_fast_share_server_response();
  fast_share_server_response->set_name(name);
  fast_share_server_response->set_status(state);
  fast_share_server_response->set_latency_millis(latency_millis);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSetDataUsage(DataUsage original_preference,
                                        DataUsage preference) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::SET_DATA_USAGE);

  auto* set_data_usage = sharing_log->mutable_set_data_usage();
  set_data_usage->set_original_preference(
      GetLoggerDataUsage(original_preference));
  set_data_usage->set_preference(GetLoggerDataUsage(preference));

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewAddQuickSettingsTile() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::ADD_QUICK_SETTINGS_TILE);

  sharing_log->mutable_add_quick_settings_tile();

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewRemoveQuickSettingsTile() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::REMOVE_QUICK_SETTINGS_TILE);

  sharing_log->mutable_remove_quick_settings_tile();

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewTapQuickSettingsTile() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::TAP_QUICK_SETTINGS_TILE);

  sharing_log->mutable_tap_quick_settings_tile();

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewToggleShowNotification(
    ShowNotificationStatus prev_status, ShowNotificationStatus current_status) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::TOGGLE_SHOW_NOTIFICATION);

  auto* toggle_show_notification =
      sharing_log->mutable_toggle_show_notification();
  toggle_show_notification->set_current_status(current_status);
  toggle_show_notification->set_previous_status(prev_status);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSetDeviceName(int device_name_size) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::SET_DEVICE_NAME);

  auto* set_device_name = sharing_log->mutable_set_device_name();
  set_device_name->set_device_name_size(device_name_size);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewRequestSettingPermissions(
    ::location::nearby::proto::sharing::PermissionRequestType type,
    ::location::nearby::proto::sharing::PermissionRequestResult result) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::REQUEST_SETTING_PERMISSIONS);

  auto* request_setting_permissions =
      sharing_log->mutable_request_setting_permissions();
  request_setting_permissions->set_permission_type(type);
  request_setting_permissions->set_permission_request_result(result);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewInstallAPKStatus(
    ::location::nearby::proto::sharing::InstallAPKStatus status,
    ::location::nearby::proto::sharing::ApkSource source) {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::RECEIVING_EVENT, EventType::INSTALL_APK);

  auto* install_apk_status = sharing_log->mutable_install_apk_status();
  install_apk_status->add_status(status);
  install_apk_status->add_source(source);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewVerifyAPKStatus(
    ::location::nearby::proto::sharing::VerifyAPKStatus status,
    ::location::nearby::proto::sharing::ApkSource source) {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::RECEIVING_EVENT, EventType::VERIFY_APK);

  auto* verify_apk_status = sharing_log->mutable_verify_apk_status();
  verify_apk_status->add_status(status);
  verify_apk_status->add_source(source);

  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewRpcCallStatus(
    absl::string_view rpc_name,
    SharingLog::RpcCallStatus::RpcDirection direction,
    int error_code, absl::Duration latency) {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::RPC_EVENT, EventType::RPC_CALL_STATUS);

  auto* rpc_call_status = sharing_log->mutable_rpc_call_status();
  rpc_call_status->set_rpc_name(std::string(rpc_name));
  rpc_call_status->set_direction(direction);
  rpc_call_status->set_error_code(error_code);
  rpc_call_status->set_latency_millis(absl::ToInt64Milliseconds(latency));

  LogEvent(*sharing_log);
}

// Start private methods.

std::unique_ptr<SharingLog> AnalyticsRecorder::CreateSharingLog(
    EventCategory event_category, EventType event_type) {
  auto sharing_log = std::make_unique<SharingLog>();
  sharing_log->set_event_category(event_category);
  sharing_log->set_event_type(event_type);
  sharing_log->mutable_event_metadata()->set_vendor_id(vendor_id_);
  return sharing_log;
}

void AnalyticsRecorder::LogEvent(const SharingLog& message) {
  if (event_logger_ == nullptr) {
    return;
  }

  event_logger_->Log(message);
}

int64_t AnalyticsRecorder::GenerateNextId() {
  absl::BitGen bit_gen;
  return absl::Uniform(bit_gen, 0, INT64_MAX - 1) + 1;
}

}  // namespace analytics
}  // namespace sharing
}  // namespace nearby
