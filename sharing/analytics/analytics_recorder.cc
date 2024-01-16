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
#include <vector>

#include "google/protobuf/duration.pb.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_device_settings.h"
#include "sharing/analytics/analytics_information.h"
#include "sharing/attachment.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/file_attachment.h"
#include "sharing/internal/public/logging.h"
#include "sharing/proto/analytics/nearby_sharing_log.pb.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/share_target.h"
#include "sharing/wifi_credentials_attachment.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace nearby {
namespace sharing {
namespace analytics {
namespace {

using ::location::nearby::proto::sharing::DesktopNotification;
using ::location::nearby::proto::sharing::DesktopTransferEventType;
using ::location::nearby::proto::sharing::DeviceRelationship;
using ::location::nearby::proto::sharing::DeviceType;
using ::location::nearby::proto::sharing::EventCategory;
using ::location::nearby::proto::sharing::EventType;
using ::location::nearby::proto::sharing::OSType;

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

::location::nearby::proto::sharing::Visibility GetLoggerVisibility(
    DeviceVisibility visibility) {
  switch (visibility) {
    case DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS:
      return ::location::nearby::proto::sharing::Visibility::CONTACTS_ONLY;
    case DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS:
      return ::location::nearby::proto::sharing::Visibility::
          SELECTED_CONTACTS_ONLY;
    case DeviceVisibility::DEVICE_VISIBILITY_EVERYONE:
      return ::location::nearby::proto::sharing::Visibility::EVERYONE;
    case DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE:
      return ::location::nearby::proto::sharing::Visibility::SELF_SHARE;
    default:
      return ::location::nearby::proto::sharing::Visibility::UNKNOWN_VISIBILITY;
  }
}

::location::nearby::proto::sharing::DataUsage GetLoggerDataUsage(
    DataUsage data_usage) {
  switch (data_usage) {
    case DataUsage::OFFLINE_DATA_USAGE:
      return ::location::nearby::proto::sharing::DataUsage::OFFLINE;
    case DataUsage::ONLINE_DATA_USAGE:
      return ::location::nearby::proto::sharing::DataUsage::ONLINE;
    case DataUsage::WIFI_ONLY_DATA_USAGE:
      return ::location::nearby::proto::sharing::DataUsage::WIFI_ONLY;
    default:
      return ::location::nearby::proto::sharing::DataUsage::UNKNOWN_DATA_USAGE;
  }
}

// TODO(b/269353084): Auto-generate codes when a new source type in
// sharing_enums.proto is added.
::location::nearby::proto::sharing::AttachmentSourceType
GetLoggerAttachmentSourceType(Attachment::SourceType source_type) {
  switch (source_type) {
    case Attachment::SourceType::kContextMenu:
      return ::location::nearby::proto::sharing::AttachmentSourceType::
          ATTACHMENT_SOURCE_CONTEXT_MENU;
    case Attachment::SourceType::kDragAndDrop:
      return ::location::nearby::proto::sharing::AttachmentSourceType::
          ATTACHMENT_SOURCE_DRAG_AND_DROP;
    case Attachment::SourceType::kSelectFilesButton:
      return ::location::nearby::proto::sharing::AttachmentSourceType::
          ATTACHMENT_SOURCE_SELECT_FILES_BUTTON;
    case Attachment::SourceType::kPaste:
      return ::location::nearby::proto::sharing::AttachmentSourceType::
          ATTACHMENT_SOURCE_PASTE;
    case Attachment::SourceType::kSelectFoldersButton:
      return ::location::nearby::proto::sharing::AttachmentSourceType::
          ATTACHMENT_SOURCE_SELECT_FOLDERS_BUTTON;
    default:
      return ::location::nearby::proto::sharing::AttachmentSourceType::
          ATTACHMENT_SOURCE_UNKNOWN;
  }
}

analytics::proto::SharingLog::ShareTargetInfo* GetAllocatedShareTargetInfo(
    ShareTargetType device_type, DeviceRelationship relationship,
    OSType os_type = OSType::UNKNOWN_OS_TYPE) {
  auto share_target_info =
      analytics::proto::SharingLog::ShareTargetInfo::default_instance().New();
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
  return share_target_info;
}

analytics::proto::SharingLog::ShareTargetInfo* GetAllocatedShareTargetInfo(
    const ShareTarget& share_target, OSType os_type = OSType::UNKNOWN_OS_TYPE) {
  auto share_target_info =
      analytics::proto::SharingLog::ShareTargetInfo::default_instance().New();
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
  return share_target_info;
}

analytics::proto::SharingLog::AttachmentsInfo* GenerateAllocatedAttachmentInfo(
    const std::vector<std::unique_ptr<Attachment>>& attachments) {
  auto attachments_info =
      analytics::proto::SharingLog::AttachmentsInfo::default_instance().New();

  for (auto& attachment : attachments) {
    int64_t size = attachment->size();
    if (attachment->family() == Attachment::Family::kText) {
      analytics::proto::SharingLog::TextAttachment::Type type =
          analytics::proto::SharingLog::TextAttachment::UNKNOWN_TEXT_TYPE;
      switch (attachment->GetShareType()) {
        case ShareType::kPhone:
          type = analytics::proto::SharingLog::TextAttachment::PHONE_NUMBER;
          break;
        case ShareType::kUrl:
          type = analytics::proto::SharingLog::TextAttachment::URL;
          break;
        case ShareType::kAddress:
          type = analytics::proto::SharingLog::TextAttachment::ADDRESS;
          break;
        case ShareType::kText:
          // Apply UNKNOWN_TEXT_TYPE for it based on analytics design.
          break;
        default:
          break;
      }

      ::google::protobuf::RepeatedPtrField<analytics::proto::SharingLog_TextAttachment>*
          text_attachments = attachments_info->mutable_text_attachment();
      analytics::proto::SharingLog_TextAttachment* text_attachment =
          analytics::proto::SharingLog::TextAttachment::default_instance()
              .New();
      text_attachment->set_type(type);
      text_attachment->set_size_bytes(size);
      text_attachment->set_source_type(
          GetLoggerAttachmentSourceType(attachment->source_type()));
      text_attachment->set_batch_id(attachment->batch_id());
      text_attachments->AddAllocated(text_attachment);
    } else if (attachment->family() == Attachment::Family::kFile) {
      analytics::proto::SharingLog::FileAttachment::Type type =
          analytics::proto::SharingLog::FileAttachment::UNKNOWN_FILE_TYPE;
      switch (attachment->GetShareType()) {
        case ShareType::kImageFile:
          type = analytics::proto::SharingLog::FileAttachment::IMAGE;
          break;
        case ShareType::kVideoFile:
          type = analytics::proto::SharingLog::FileAttachment::VIDEO;
          break;
        case ShareType::kAudioFile:
          type = analytics::proto::SharingLog::FileAttachment::AUDIO;
          break;
        case ShareType::kPdfFile:
        case ShareType::kTextFile:
        case ShareType::kGoogleDocsFile:
        case ShareType::kGoogleSheetsFile:
        case ShareType::kGoogleSlidesFile:
          type = analytics::proto::SharingLog::FileAttachment::DOCUMENT;
          break;
        case ShareType::kUnknownFile:
          // The default type is set to type.
          break;
        default:
          break;
      }

      ::google::protobuf::RepeatedPtrField<analytics::proto::SharingLog_FileAttachment>*
          file_attachments = attachments_info->mutable_file_attachment();
      analytics::proto::SharingLog_FileAttachment* file_attachment =
          analytics::proto::SharingLog::FileAttachment::default_instance()
              .New();
      file_attachment->set_type(type);
      file_attachment->set_size_bytes(size);
      file_attachment->set_offset_bytes(0);
      file_attachment->set_source_type(
          GetLoggerAttachmentSourceType(attachment->source_type()));
      file_attachment->set_batch_id(attachment->batch_id());
      file_attachments->AddAllocated(file_attachment);
    } else if (attachment->family() == Attachment::Family::kWifiCredentials) {
      ::google::protobuf::RepeatedPtrField<
          analytics::proto::SharingLog_WifiCredentialsAttachment>*
          wifi_credentials_attachments =
              attachments_info->mutable_wifi_credentials_attachment();
      analytics::proto::SharingLog_WifiCredentialsAttachment*
          wifi_credentials_attachment =
              analytics::proto::SharingLog::WifiCredentialsAttachment::
                  default_instance()
                      .New();
      wifi_credentials_attachment->set_security_type(
          dynamic_cast<WifiCredentialsAttachment*>(attachment.get())
              ->security_type());
      wifi_credentials_attachment->set_source_type(
          GetLoggerAttachmentSourceType(attachment->source_type()));
      wifi_credentials_attachment->set_batch_id(attachment->batch_id());
      wifi_credentials_attachments->AddAllocated(wifi_credentials_attachment);
    } else {
      NL_LOG(WARNING)
          << __func__
          << "Unable to create event for attachment info. Unknown attachment "
          << attachment->id();
      continue;
    }
  }

  if (attachments_info->file_attachment_size() == 0 &&
      attachments_info->text_attachment_size() == 0 &&
      attachments_info->wifi_credentials_attachment_size() == 0) {
    std::string type =
        attachments.empty()
            ? "NULL"
            : absl::StrCat(static_cast<int>(attachments[0]->GetShareType()));
    NL_LOG(WARNING) << __func__ << "attachmentInfo is empty, attachment size="
                    << attachments.size() << ", type=" << type;
  }

  return attachments_info;
}

}  // namespace

void AnalyticsRecorder::NewEstablishConnection(
    int64_t session_id,
    ::location::nearby::proto::sharing::EstablishConnectionStatus
        connection_status,
    ShareTarget share_target, int transfer_position, int concurrent_connections,
    int64_t duration_millis, std::optional<std::string> referrer_package) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::ESTABLISH_CONNECTION);

  auto establish_connection =
      analytics::proto::SharingLog::EstablishConnection::default_instance()
          .New();

  establish_connection->set_session_id(session_id);
  establish_connection->set_status(connection_status);
  establish_connection->set_allocated_share_target_info(
      GetAllocatedShareTargetInfo(share_target));
  establish_connection->set_transfer_position(transfer_position);
  establish_connection->set_concurrent_connections(concurrent_connections);
  establish_connection->set_duration_millis(duration_millis);
  if (referrer_package.has_value()) {
    establish_connection->set_referrer_name(*referrer_package);
  }

  sharing_log->set_allocated_establish_connection(establish_connection);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewAcceptAgreements() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::ACCEPT_AGREEMENTS);

  auto accept_agreements =
      analytics::proto::SharingLog::AcceptAgreements::default_instance().New();

  sharing_log->set_allocated_accept_agreements(accept_agreements);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewDeclineAgreements() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::DECLINE_AGREEMENTS);

  auto decline_agreements =
      analytics::proto::SharingLog::DeclineAgreements::default_instance().New();

  sharing_log->set_allocated_decline_agreements(decline_agreements);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewAddContact() {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::SETTINGS_EVENT, EventType::ADD_CONTACT);

  auto add_contact =
      analytics::proto::SharingLog::AddContact::default_instance().New();

  sharing_log->set_allocated_add_contact(add_contact);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewRemoveContact() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::REMOVE_CONTACT);

  auto remove_contact =
      analytics::proto::SharingLog::RemoveContact::default_instance().New();

  sharing_log->set_allocated_remove_contact(remove_contact);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewTapFeedback() {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::SETTINGS_EVENT, EventType::TAP_FEEDBACK);

  auto tap_feedback =
      analytics::proto::SharingLog::TapFeedback::default_instance().New();

  sharing_log->set_allocated_tap_feedback(tap_feedback);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewTapHelp() {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::SETTINGS_EVENT, EventType::TAP_HELP);

  auto tap_help =
      analytics::proto::SharingLog::TapHelp::default_instance().New();

  sharing_log->set_allocated_tap_help(tap_help);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewLaunchDeviceContactConsent(
    ::location::nearby::proto::sharing::ConsentAcceptanceStatus status) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::LAUNCH_CONSENT);

  auto launch_consent =
      analytics::proto::SharingLog::LaunchConsent::default_instance().New();
  launch_consent->set_status(status);

  sharing_log->set_allocated_launch_consent(launch_consent);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewAdvertiseDevicePresenceEnd(int64_t session_id) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::ADVERTISE_DEVICE_PRESENCE_END);

  auto advertise_device_presence_end =
      analytics::proto::SharingLog::AdvertiseDevicePresenceEnd::
          default_instance()
              .New();
  advertise_device_presence_end->set_session_id(session_id);

  sharing_log->set_allocated_advertise_device_presence_end(
      advertise_device_presence_end);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewAdvertiseDevicePresenceStart(
    int64_t session_id, DeviceVisibility visibility,
    ::location::nearby::proto::sharing::SessionStatus status,
    DataUsage data_usage, std::optional<std::string> referrer_package) {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::RECEIVING_EVENT,
                       EventType::ADVERTISE_DEVICE_PRESENCE_START);

  auto advertise_device_presence_start =
      analytics::proto::SharingLog::AdvertiseDevicePresenceStart::
          default_instance()
              .New();
  advertise_device_presence_start->set_session_id(session_id);
  advertise_device_presence_start->set_visibility(
      GetLoggerVisibility(visibility));
  advertise_device_presence_start->set_status(status);
  advertise_device_presence_start->set_data_usage(
      GetLoggerDataUsage(data_usage));
  if (referrer_package.has_value()) {
    advertise_device_presence_start->set_referrer_name(*referrer_package);
  }

  sharing_log->set_allocated_advertise_device_presence_start(
      advertise_device_presence_start);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewDescribeAttachments(
    const std::vector<std::unique_ptr<Attachment>>& attachments) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::DESCRIBE_ATTACHMENTS);

  auto describe_attachments =
      analytics::proto::SharingLog::DescribeAttachments::default_instance()
          .New();
  describe_attachments->set_allocated_attachments_info(
      GenerateAllocatedAttachmentInfo(attachments));

  sharing_log->set_allocated_describe_attachments(describe_attachments);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewDiscoverShareTarget(
    ShareTarget share_target, int64_t session_id,
    int64_t latency_since_scanning_start_millis, int64_t flow_id,
    std::optional<std::string> referrer_package,
    int64_t latency_since_send_surface_registered_millis) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::DISCOVER_SHARE_TARGET);

  auto discover_share_target =
      analytics::proto::SharingLog::DiscoverShareTarget::default_instance()
          .New();
  discover_share_target->set_session_id(session_id);
  auto duration = google::protobuf::Duration::default_instance().New();
  duration->set_seconds(latency_since_scanning_start_millis / 1000);
  duration->set_nanos((latency_since_scanning_start_millis % 1000) * 1000000);
  discover_share_target->set_allocated_duration_since_scanning(duration);
  discover_share_target->set_allocated_share_target_info(
      GetAllocatedShareTargetInfo(share_target));
  discover_share_target->set_session_id(session_id);
  discover_share_target->set_flow_id(flow_id);

  discover_share_target->set_latency_since_activity_start_millis(
      latency_since_send_surface_registered_millis > 0
          ? latency_since_send_surface_registered_millis
          : -1);

  sharing_log->set_allocated_discover_share_target(discover_share_target);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewEnableNearbySharing(
    ::location::nearby::proto::sharing::NearbySharingStatus status) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::ENABLE_NEARBY_SHARING);

  auto enable_nearby_sharing =
      analytics::proto::SharingLog::EnableNearbySharing::default_instance()
          .New();
  enable_nearby_sharing->set_status(status);

  sharing_log->set_allocated_enable_nearby_sharing(enable_nearby_sharing);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewOpenReceivedAttachments(
    const std::vector<std::unique_ptr<Attachment>>& attachments,
    int64_t session_id) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::OPEN_RECEIVED_ATTACHMENTS);

  auto open_received_attachments =
      analytics::proto::SharingLog::OpenReceivedAttachments::default_instance()
          .New();
  open_received_attachments->set_allocated_attachments_info(
      GenerateAllocatedAttachmentInfo(attachments));
  open_received_attachments->set_session_id(session_id);

  sharing_log->set_allocated_open_received_attachments(
      open_received_attachments);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewProcessReceivedAttachmentsEnd(
    int64_t session_id,
    ::location::nearby::proto::sharing::ProcessReceivedAttachmentsStatus
        status) {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::RECEIVING_EVENT,
                       EventType::PROCESS_RECEIVED_ATTACHMENTS_END);

  auto process_received_attachments_end =
      analytics::proto::SharingLog::ProcessReceivedAttachmentsEnd::
          default_instance()
              .New();
  process_received_attachments_end->set_status(status);
  process_received_attachments_end->set_session_id(session_id);

  sharing_log->set_allocated_process_received_attachments_end(
      process_received_attachments_end);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewReceiveAttachmentsEnd(
    int64_t session_id, int64_t received_bytes,
    ::location::nearby::proto::sharing::AttachmentTransmissionStatus status,
    std::optional<std::string> referrer_package) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::RECEIVE_ATTACHMENTS_END);

  auto receive_attachments_end =
      analytics::proto::SharingLog::ReceiveAttachmentsEnd::default_instance()
          .New();
  receive_attachments_end->set_session_id(session_id);
  receive_attachments_end->set_received_bytes(received_bytes);
  receive_attachments_end->set_status(status);
  if (referrer_package.has_value()) {
    receive_attachments_end->set_referrer_name(*referrer_package);
  }

  sharing_log->set_allocated_receive_attachments_end(receive_attachments_end);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewReceiveAttachmentsStart(
    int64_t session_id,
    const std::vector<std::unique_ptr<Attachment>>& attachments) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::RECEIVE_ATTACHMENTS_START);

  auto receive_attachments_start =
      analytics::proto::SharingLog::ReceiveAttachmentsStart::default_instance()
          .New();
  receive_attachments_start->set_allocated_attachments_info(
      GenerateAllocatedAttachmentInfo(attachments));
  receive_attachments_start->set_session_id(session_id);

  sharing_log->set_allocated_receive_attachments_start(
      receive_attachments_start);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewReceiveFastInitialization(
    int64_t timeElapseSinceScreenUnlockMillis) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::RECEIVE_FAST_INITIALIZATION);

  auto receive_fast_initialization =
      analytics::proto::SharingLog::ReceiveFastInitialization::
          default_instance()
              .New();

  receive_fast_initialization->set_time_elapse_since_screen_unlock_millis(
      timeElapseSinceScreenUnlockMillis);

  sharing_log->set_allocated_receive_initialization(
      receive_fast_initialization);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewAcceptFastInitialization() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::ACCEPT_FAST_INITIALIZATION);

  auto accept_fast_initialization =
      analytics::proto::SharingLog::AcceptFastInitialization::default_instance()
          .New();

  sharing_log->set_allocated_accept_fast_initialization(
      accept_fast_initialization);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewDismissFastInitialization() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::DISMISS_FAST_INITIALIZATION);

  auto dismiss_fast_initialization =
      analytics::proto::SharingLog::DismissFastInitialization::
          default_instance()
              .New();

  sharing_log->set_allocated_dismiss_fast_initialization(
      dismiss_fast_initialization);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewReceiveIntroduction(
    int64_t session_id, ShareTarget share_target,
    std::optional<std::string> referrer_package,
    ::location::nearby::proto::sharing::OSType share_target_os_type) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::RECEIVE_INTRODUCTION);

  auto receive_introduction =
      analytics::proto::SharingLog::ReceiveIntroduction::default_instance()
          .New();
  receive_introduction->set_session_id(session_id);
  receive_introduction->set_allocated_share_target_info(
      GetAllocatedShareTargetInfo(share_target, share_target_os_type));
  if (referrer_package.has_value()) {
    receive_introduction->set_referrer_name(*referrer_package);
  }

  sharing_log->set_allocated_receive_introduction(receive_introduction);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewRespondToIntroduction(
    ::location::nearby::proto::sharing::ResponseToIntroduction action,
    int64_t session_id) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::RESPOND_TO_INTRODUCTION);

  auto respond_to_introduction =
      analytics::proto::SharingLog::RespondToIntroduction::default_instance()
          .New();
  respond_to_introduction->set_session_id(session_id);
  respond_to_introduction->set_action(action);

  sharing_log->set_allocated_respond_introduction(respond_to_introduction);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewTapPrivacyNotification() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::TAP_PRIVACY_NOTIFICATION);

  auto tap_privacy_notification =
      analytics::proto::SharingLog::TapPrivacyNotification::default_instance()
          .New();

  sharing_log->set_allocated_tap_privacy_notification(tap_privacy_notification);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewDismissPrivacyNotification() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::RECEIVING_EVENT, EventType::DISMISS_PRIVACY_NOTIFICATION);

  auto dismiss_privacy_notification =
      analytics::proto::SharingLog::DismissPrivacyNotification::
          default_instance()
              .New();

  sharing_log->set_allocated_dismiss_privacy_notification(
      dismiss_privacy_notification);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewScanForShareTargetsEnd(int64_t session_id) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SCAN_FOR_SHARE_TARGETS_END);

  auto scan_for_share_targets_end =
      analytics::proto::SharingLog::ScanForShareTargetsEnd::default_instance()
          .New();
  scan_for_share_targets_end->set_session_id(session_id);

  sharing_log->set_allocated_scan_for_share_targets_end(
      scan_for_share_targets_end);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewScanForShareTargetsStart(
    int64_t session_id,
    ::location::nearby::proto::sharing::SessionStatus status,
    AnalyticsInformation analytics_information, int64_t flow_id,
    std::optional<std::string> referrer_package) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SCAN_FOR_SHARE_TARGETS_START);

  auto scan_for_share_targets_start =
      analytics::proto::SharingLog::ScanForShareTargetsStart::default_instance()
          .New();
  scan_for_share_targets_start->set_session_id(session_id);
  scan_for_share_targets_start->set_status(status);
  scan_for_share_targets_start->set_scan_type(
      static_cast<::location::nearby::proto::sharing::ScanType>(
          analytics_information.send_surface_state));
  scan_for_share_targets_start->set_flow_id(flow_id);
  if (referrer_package.has_value()) {
    scan_for_share_targets_start->set_referrer_name(*referrer_package);
  }

  sharing_log->set_allocated_scan_for_share_targets_start(
      scan_for_share_targets_start);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendAttachmentsEnd(
    int64_t session_id, int64_t sent_bytes, ShareTarget share_target,
    ::location::nearby::proto::sharing::AttachmentTransmissionStatus status,
    int transfer_position, int concurrent_connections, int64_t duration_millis,
    std::optional<std::string> referrer_package,
    ::location::nearby::proto::sharing::ConnectionLayerStatus
        connection_layer_status,
    ::location::nearby::proto::sharing::OSType share_target_os_type) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SEND_ATTACHMENTS_END);

  auto send_attachments_end =
      analytics::proto::SharingLog::SendAttachmentsEnd::default_instance()
          .New();
  send_attachments_end->set_session_id(session_id);
  send_attachments_end->set_sent_bytes(sent_bytes);
  send_attachments_end->set_allocated_share_target_info(
      GetAllocatedShareTargetInfo(share_target, share_target_os_type));
  send_attachments_end->set_status(status);
  send_attachments_end->set_transfer_position(transfer_position);
  send_attachments_end->set_concurrent_connections(concurrent_connections);
  send_attachments_end->set_duration_millis(duration_millis);
  if (referrer_package.has_value()) {
    send_attachments_end->set_referrer_name(*referrer_package);
  }
  send_attachments_end->set_connection_layer_status(connection_layer_status);

  sharing_log->set_allocated_send_attachments_end(send_attachments_end);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendAttachmentsStart(
    int64_t session_id,
    const std::vector<std::unique_ptr<Attachment>>& attachments,
    int transfer_position, int concurrent_connections) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SEND_ATTACHMENTS_START);

  auto send_attachments_start =
      analytics::proto::SharingLog::SendAttachmentsStart::default_instance()
          .New();
  send_attachments_start->set_session_id(session_id);
  send_attachments_start->set_allocated_attachments_info(
      GenerateAllocatedAttachmentInfo(attachments));
  send_attachments_start->set_transfer_position(transfer_position);
  send_attachments_start->set_concurrent_connections(concurrent_connections);

  sharing_log->set_allocated_send_attachments_start(send_attachments_start);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendFastInitialization() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SEND_FAST_INITIALIZATION);

  auto send_fast_initialization =
      analytics::proto::SharingLog::SendFastInitialization::default_instance()
          .New();

  sharing_log->set_allocated_send_initialization(send_fast_initialization);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendStart(int64_t session_id, int transfer_position,
                                     int concurrent_connections,
                                     ShareTarget share_target) {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::SENDING_EVENT, EventType::SEND_START);

  auto send_start =
      analytics::proto::SharingLog::SendStart::default_instance().New();
  send_start->set_session_id(session_id);
  send_start->set_transfer_position(transfer_position);
  send_start->set_concurrent_connections(concurrent_connections);
  send_start->set_allocated_share_target_info(
      GetAllocatedShareTargetInfo(share_target));

  sharing_log->set_allocated_send_start(send_start);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendIntroduction(
    ShareTargetType target_type, int64_t session_id,
    DeviceRelationship relationship,
    ::location::nearby::proto::sharing::OSType share_target_os_type) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SEND_INTRODUCTION);
  auto send_introduction =
      analytics::proto::SharingLog::SendIntroduction::default_instance().New();
  send_introduction->set_allocated_share_target_info(
      GetAllocatedShareTargetInfo(target_type, relationship,
                                  share_target_os_type));
  send_introduction->set_session_id(session_id);

  sharing_log->set_allocated_send_introduction(send_introduction);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendIntroduction(
    int64_t session_id, ShareTarget share_target, int transfer_position,
    int concurrent_connections,
    ::location::nearby::proto::sharing::OSType share_target_os_type) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SENDING_EVENT, EventType::SEND_INTRODUCTION);

  auto send_introduction =
      analytics::proto::SharingLog::SendIntroduction::default_instance().New();
  send_introduction->set_allocated_share_target_info(
      GetAllocatedShareTargetInfo(share_target, share_target_os_type));
  send_introduction->set_session_id(session_id);
  send_introduction->set_transfer_position(transfer_position);
  send_introduction->set_concurrent_connections(concurrent_connections);

  sharing_log->set_allocated_send_introduction(send_introduction);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSetVisibility(DeviceVisibility src_visibility,
                                         DeviceVisibility dst_visibility,
                                         int64_t duration_millis) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::SET_VISIBILITY);

  auto set_visibility =
      analytics::proto::SharingLog::SetVisibility::default_instance().New();
  set_visibility->set_visibility(GetLoggerVisibility(dst_visibility));
  set_visibility->set_source_visibility(GetLoggerVisibility(src_visibility));
  set_visibility->set_duration_millis(duration_millis);

  sharing_log->set_allocated_set_visibility(set_visibility);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewDeviceSettings(AnalyticsDeviceSettings settings) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::DEVICE_SETTINGS);

  auto device_settings =
      analytics::proto::SharingLog::DeviceSettings::default_instance().New();
  device_settings->set_data_usage(GetLoggerDataUsage(settings.data_usage));
  device_settings->set_device_name_size(settings.device_name_size);
  device_settings->set_is_show_notification_enabled(
      settings.is_fast_init_notification_enabled);
  device_settings->set_visibility(GetLoggerVisibility(settings.visibility));

  sharing_log->set_allocated_device_settings(device_settings);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewFastShareServerResponse(
    ::location::nearby::proto::sharing::ServerActionName name,
    ::location::nearby::proto::sharing::ServerResponseState state,
    int64_t latency_millis) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::FAST_SHARE_SERVER_RESPONSE);

  auto fast_share_server_response =
      analytics::proto::SharingLog::FastShareServerResponse::default_instance()
          .New();
  fast_share_server_response->set_name(name);
  fast_share_server_response->set_status(state);
  fast_share_server_response->set_latency_millis(latency_millis);

  sharing_log->set_allocated_fast_share_server_response(
      fast_share_server_response);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSetDataUsage(DataUsage original_preference,
                                        DataUsage preference) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::SET_DATA_USAGE);

  auto set_data_usage =
      analytics::proto::SharingLog::SetDataUsage::default_instance().New();
  set_data_usage->set_original_preference(
      GetLoggerDataUsage(original_preference));
  set_data_usage->set_preference(GetLoggerDataUsage(preference));

  sharing_log->set_allocated_set_data_usage(set_data_usage);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewAddQuickSettingsTile() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::ADD_QUICK_SETTINGS_TILE);

  auto add_quick_settings_tile =
      analytics::proto::SharingLog::AddQuickSettingsTile::default_instance()
          .New();

  sharing_log->set_allocated_add_quick_settings_tile(add_quick_settings_tile);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewRemoveQuickSettingsTile() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::REMOVE_QUICK_SETTINGS_TILE);

  auto remove_quick_settings_tile =
      analytics::proto::SharingLog::RemoveQuickSettingsTile::default_instance()
          .New();

  sharing_log->set_allocated_remove_quick_settings_tile(
      remove_quick_settings_tile);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewTapQuickSettingsTile() {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::TAP_QUICK_SETTINGS_TILE);

  auto tap_quick_settings_tile =
      analytics::proto::SharingLog::TapQuickSettingsTile::default_instance()
          .New();

  sharing_log->set_allocated_tap_quick_settings_tile(tap_quick_settings_tile);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewToggleShowNotification(
    ::location::nearby::proto::sharing::ShowNotificationStatus prev_status,
    ::location::nearby::proto::sharing::ShowNotificationStatus current_status) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::TOGGLE_SHOW_NOTIFICATION);

  auto toggle_show_notification =
      analytics::proto::SharingLog::ToggleShowNotification::default_instance()
          .New();
  toggle_show_notification->set_current_status(current_status);
  toggle_show_notification->set_previous_status(prev_status);

  sharing_log->set_allocated_toggle_show_notification(toggle_show_notification);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSetDeviceName(int device_name_size) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::SET_DEVICE_NAME);

  auto set_device_name =
      analytics::proto::SharingLog::SetDeviceName::default_instance().New();
  set_device_name->set_device_name_size(device_name_size);

  sharing_log->set_allocated_set_device_name(set_device_name);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewRequestSettingPermissions(
    ::location::nearby::proto::sharing::PermissionRequestType type,
    ::location::nearby::proto::sharing::PermissionRequestResult result) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::REQUEST_SETTING_PERMISSIONS);

  auto request_setting_permissions =
      analytics::proto::SharingLog::RequestSettingPermissions::
          default_instance()
              .New();
  request_setting_permissions->set_permission_type(type);
  request_setting_permissions->set_permission_request_result(result);

  sharing_log->set_allocated_request_setting_permissions(
      request_setting_permissions);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewInstallAPKStatus(
    ::location::nearby::proto::sharing::InstallAPKStatus status,
    ::location::nearby::proto::sharing::ApkSource source) {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::RECEIVING_EVENT, EventType::INSTALL_APK);

  auto install_apk_status =
      analytics::proto::SharingLog::InstallAPKStatus::default_instance().New();
  install_apk_status->add_status(status);
  install_apk_status->add_source(source);

  sharing_log->set_allocated_install_apk_status(install_apk_status);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewVerifyAPKStatus(
    ::location::nearby::proto::sharing::VerifyAPKStatus status,
    ::location::nearby::proto::sharing::ApkSource source) {
  std::unique_ptr<SharingLog> sharing_log =
      CreateSharingLog(EventCategory::RECEIVING_EVENT, EventType::VERIFY_APK);

  auto verify_apk_status =
      analytics::proto::SharingLog::VerifyAPKStatus::default_instance().New();
  verify_apk_status->add_status(status);
  verify_apk_status->add_source(source);

  sharing_log->set_allocated_verify_apk_status(verify_apk_status);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendDesktopNotification(DesktopNotification event) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::SEND_DESKTOP_NOTIFICATION);

  auto send_desktop_notification =
      analytics::proto::SharingLog::SendDesktopNotification::default_instance()
          .New();
  send_desktop_notification->set_event(event);

  sharing_log->set_allocated_send_desktop_notification(
      send_desktop_notification);
  LogEvent(*sharing_log);
}

void AnalyticsRecorder::NewSendDesktopTransferEvent(
    DesktopTransferEventType event) {
  std::unique_ptr<SharingLog> sharing_log = CreateSharingLog(
      EventCategory::SETTINGS_EVENT, EventType::SEND_DESKTOP_TRANSFER_EVENT);

  auto send_desktop_transfer_event =
      analytics::proto::SharingLog::SendDesktopTransferEvent::default_instance()
          .New();
  send_desktop_transfer_event->set_event(event);

  sharing_log->set_allocated_send_desktop_transfer_event(
      send_desktop_transfer_event);
  LogEvent(*sharing_log);
}

// Start private methods.

std::unique_ptr<SharingLog> AnalyticsRecorder::CreateSharingLog(
    EventCategory event_category, EventType event_type) {
  auto sharing_log = std::make_unique<SharingLog>();
  sharing_log->set_event_category(event_category);
  sharing_log->set_event_type(event_type);
  return sharing_log;
}

void AnalyticsRecorder::LogEvent(const ::google::protobuf::MessageLite& message) {
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
