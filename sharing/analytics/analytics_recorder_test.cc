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

#include <stdint.h>

#include <optional>
#include <string>

#include "google/protobuf/duration.pb.h"
#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "internal/analytics/mock_event_logger.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_device_settings.h"
#include "sharing/analytics/analytics_information.h"
#include "sharing/attachment_container.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/file_attachment.h"
#include "sharing/proto/analytics/nearby_sharing_log.pb.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/text_attachment.h"

namespace nearby {
namespace sharing {
namespace analytics {
namespace {

using ::location::nearby::proto::sharing::EventCategory;
using ::location::nearby::proto::sharing::EventType;
using ::location::nearby::proto::sharing::OSType;
using ::nearby::analytics::MockEventLogger;
using ::nearby::sharing::analytics::proto::SharingLog;
using ::nearby::sharing::proto::DataUsage;
using ::nearby::sharing::proto::DeviceVisibility;
using ::testing::An;

constexpr absl::string_view kFileName = "fileName";
constexpr absl::string_view kTextBody = "textBody";
constexpr absl::string_view kFileDocumentName = "abc.pdf";
constexpr absl::string_view kFileMimeType = "application/pdf";
constexpr absl::string_view kTextMimeType = "text/plain";
constexpr absl::string_view kAppPackageName = "com.google.android.youtube";

class AnalyticsRecorderTest : public ::testing::Test {
 public:
  AnalyticsRecorderTest() = default;
  ~AnalyticsRecorderTest() override = default;

  MockEventLogger& event_logger() { return event_logger_; }

  AnalyticsRecorder analytics_recoder() { return analytics_recorder_; }

 private:
  MockEventLogger event_logger_;
  AnalyticsRecorder analytics_recorder_{/*vendor_id=*/0, &event_logger_};
};

TEST_F(AnalyticsRecorderTest, NewEstablishConnection) {
  ShareTarget share_target;
  share_target.device_name = "share_target";
  share_target.type = ShareTargetType::kPhone;

  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::ESTABLISH_CONNECTION);
        EXPECT_EQ(log.event_category(), EventCategory::SENDING_EVENT);
        EXPECT_EQ(log.establish_connection().status(),
                  location::nearby::proto::sharing::EstablishConnectionStatus::
                      CONNECTION_STATUS_SUCCESS);
        EXPECT_EQ(log.establish_connection().session_id(), 1);
        EXPECT_EQ(log.establish_connection().transfer_position(), 1);
        EXPECT_EQ(log.establish_connection().concurrent_connections(), 1);
        EXPECT_EQ(log.establish_connection().duration_millis(), 100);
        EXPECT_EQ(log.establish_connection().share_target_info().os_type(),
                  location::nearby::proto::sharing::OSType::ANDROID);
        EXPECT_EQ(log.establish_connection().referrer_name(), kAppPackageName);
      });

  analytics_recoder().NewEstablishConnection(
      1,
      location::nearby::proto::sharing::EstablishConnectionStatus::
          CONNECTION_STATUS_SUCCESS,
      share_target, 1, 1, 100, std::string(kAppPackageName));
}

TEST_F(AnalyticsRecorderTest, NewAcceptAgreements) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::ACCEPT_AGREEMENTS);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
      });

  analytics_recoder().NewAcceptAgreements();
}

TEST_F(AnalyticsRecorderTest, NewDeclineAgreements) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::DECLINE_AGREEMENTS);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
      });

  analytics_recoder().NewDeclineAgreements();
}

TEST_F(AnalyticsRecorderTest, NewAddContact) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::ADD_CONTACT);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
      });

  analytics_recoder().NewAddContact();
}

TEST_F(AnalyticsRecorderTest, NewRemoveContact) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::REMOVE_CONTACT);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
      });

  analytics_recoder().NewRemoveContact();
}

TEST_F(AnalyticsRecorderTest, NewTapFeedback) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::TAP_FEEDBACK);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
      });

  analytics_recoder().NewTapFeedback();
}

TEST_F(AnalyticsRecorderTest, NewTapHelp) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::TAP_HELP);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
      });

  analytics_recoder().NewTapHelp();
}

TEST_F(AnalyticsRecorderTest, NewLaunchDeviceContactConsent) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::LAUNCH_CONSENT);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
        EXPECT_EQ(log.launch_consent().status(),
                  location::nearby::proto::sharing::ConsentAcceptanceStatus::
                      CONSENT_ACCEPTED);
      });

  analytics_recoder().NewLaunchDeviceContactConsent(
      ::location::nearby::proto::sharing::ConsentAcceptanceStatus::
          CONSENT_ACCEPTED);
}

TEST_F(AnalyticsRecorderTest, NewAdvertiseDevicePresenceEnd) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::ADVERTISE_DEVICE_PRESENCE_END);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
        EXPECT_EQ(log.advertise_device_presence_end().session_id(), 100);
      });

  analytics_recoder().NewAdvertiseDevicePresenceEnd(100);
}

TEST_F(AnalyticsRecorderTest, NewAdvertiseDevicePresenceStart) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::ADVERTISE_DEVICE_PRESENCE_START);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
        EXPECT_EQ(log.advertise_device_presence_start().visibility(),
                  location::nearby::proto::sharing::Visibility::CONTACTS_ONLY);
        EXPECT_EQ(log.advertise_device_presence_start().status(),
                  location::nearby::proto::sharing::SessionStatus::
                      SUCCEEDED_SESSION_STATUS);
        EXPECT_EQ(log.advertise_device_presence_start().data_usage(),
                  location::nearby::proto::sharing::DataUsage::OFFLINE);
        EXPECT_EQ(log.advertise_device_presence_start().referrer_name(),
                  kAppPackageName);
        EXPECT_EQ(log.advertise_device_presence_start().session_id(), 100);
      });

  analytics_recoder().NewAdvertiseDevicePresenceStart(
      100, DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
      location::nearby::proto::sharing::SessionStatus::SUCCEEDED_SESSION_STATUS,
      DataUsage::OFFLINE_DATA_USAGE, std::string(kAppPackageName));
}

TEST_F(AnalyticsRecorderTest, NewDescribeAttachments) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::DESCRIBE_ATTACHMENTS);
        EXPECT_EQ(log.event_category(), EventCategory::SENDING_EVENT);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .text_attachment_size(),
                  5);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .text_attachment(0)
                      .size_bytes(),
                  kTextBody.size());
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .text_attachment(0)
                      .type(),
                  SharingLog::TextAttachment::UNKNOWN_TEXT_TYPE);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .text_attachment(1)
                      .type(),
                  SharingLog::TextAttachment::PHONE_NUMBER);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .text_attachment(2)
                      .type(),
                  SharingLog::TextAttachment::URL);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .text_attachment(3)
                      .type(),
                  SharingLog::TextAttachment::ADDRESS);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .text_attachment(4)
                      .type(),
                  SharingLog::TextAttachment::UNKNOWN_TEXT_TYPE);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .file_attachment_size(),
                  4);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .file_attachment(0)
                      .size_bytes(),
                  2);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .file_attachment(0)
                      .type(),
                  SharingLog::FileAttachment::IMAGE);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .file_attachment(1)
                      .type(),
                  SharingLog::FileAttachment::DOCUMENT);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .file_attachment(2)
                      .type(),
                  SharingLog::FileAttachment::AUDIO);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .file_attachment(3)
                      .type(),
                  SharingLog::FileAttachment::DOCUMENT);
      });

  AttachmentContainer attachments(
      {TextAttachment(5, service::proto::TextMetadata::TEXT,
                      std::string(kTextBody), kTextBody.size()),
       TextAttachment(6, service::proto::TextMetadata::PHONE_NUMBER,
                      std::string(kTextBody), kTextBody.size()),
       TextAttachment(7, service::proto::TextMetadata::URL,
                      std::string(kTextBody), kTextBody.size()),
       TextAttachment(8, service::proto::TextMetadata::ADDRESS,
                      std::string(kTextBody), kTextBody.size()),
       TextAttachment(9, service::proto::TextMetadata::UNKNOWN,
                      std::string(kTextBody), kTextBody.size())},
      {FileAttachment(1, 2, std::string(kFileName), "",
                      service::proto::FileMetadata::IMAGE),
       FileAttachment(2, 3, std::string(kFileDocumentName),
                      std::string(kFileMimeType),
                      service::proto::FileMetadata::DOCUMENT),
       FileAttachment(3, 4, std::string(kFileName), "",
                      service::proto::FileMetadata::AUDIO),
       FileAttachment(4, 5, std::string(kFileName), std::string(kTextMimeType),
                      service::proto::FileMetadata::DOCUMENT)},
      {});

  analytics_recoder().NewDescribeAttachments(attachments);
}

TEST_F(AnalyticsRecorderTest, EmptyDescribeAttachments) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::DESCRIBE_ATTACHMENTS);
        EXPECT_EQ(log.event_category(), EventCategory::SENDING_EVENT);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .text_attachment_size(),
                  0);
        EXPECT_EQ(log.describe_attachments()
                      .attachments_info()
                      .file_attachment_size(),
                  0);
      });

  analytics_recoder().NewDescribeAttachments(AttachmentContainer());
}

TEST_F(AnalyticsRecorderTest, NewDiscoverShareTarget) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::DISCOVER_SHARE_TARGET);
        EXPECT_EQ(log.event_category(), EventCategory::SENDING_EVENT);
        EXPECT_EQ(log.discover_share_target().duration_since_scanning().nanos(),
                  (2100 % 1000) * 1000000);
        EXPECT_EQ(
            log.discover_share_target().duration_since_scanning().seconds(),
            2100 / 1000);
        EXPECT_EQ(
            log.discover_share_target()
                .share_target_info()
                .device_relationship(),
            ::location::nearby::proto::sharing::DeviceRelationship::IS_CONTACT);
        EXPECT_EQ(log.discover_share_target().share_target_info().device_type(),
                  ::location::nearby::proto::sharing::DeviceType::LAPTOP);
        EXPECT_EQ(log.discover_share_target().share_target_info().os_type(),
                  ::location::nearby::proto::sharing::OSType::UNKNOWN_OS_TYPE);
        EXPECT_EQ(log.discover_share_target().session_id(), 1);
        EXPECT_EQ(log.discover_share_target().flow_id(), 100);
        EXPECT_FALSE(log.discover_share_target().has_referrer_name());
        EXPECT_EQ(
            log.discover_share_target().latency_since_activity_start_millis(),
            2);
      });

  ShareTarget share_target;
  share_target.device_name = "share_target";
  share_target.type = ShareTargetType::kLaptop;
  share_target.is_incoming = true;
  share_target.is_known = true;

  analytics_recoder().NewDiscoverShareTarget(share_target, 1, 2100, 100,
                                             std::nullopt, 2);
}

TEST_F(AnalyticsRecorderTest, NewEnableNearbySharing) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::ENABLE_NEARBY_SHARING);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
        EXPECT_EQ(log.enable_nearby_sharing().status(),
                  location::nearby::proto::sharing::NearbySharingStatus::ON);
      });

  analytics_recoder().NewEnableNearbySharing(
      ::location::nearby::proto::sharing::NearbySharingStatus::ON);
}

TEST_F(AnalyticsRecorderTest, NewOpenReceivedAttachments) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::OPEN_RECEIVED_ATTACHMENTS);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
        EXPECT_EQ(log.open_received_attachments()
                      .attachments_info()
                      .text_attachment_size(),
                  0);
        EXPECT_EQ(log.open_received_attachments()
                      .attachments_info()
                      .file_attachment_size(),
                  0);
        EXPECT_EQ(log.open_received_attachments().session_id(), 1);
      });

  analytics_recoder().NewOpenReceivedAttachments(AttachmentContainer(), 1);
}

TEST_F(AnalyticsRecorderTest, NewProcessReceivedAttachmentsEnd) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(),
                  EventType::PROCESS_RECEIVED_ATTACHMENTS_END);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
        EXPECT_EQ(log.process_received_attachments_end().session_id(), 1);
        EXPECT_EQ(
            log.process_received_attachments_end().status(),
            location::nearby::proto::sharing::ProcessReceivedAttachmentsStatus::
                PROCESSING_STATUS_COMPLETE_PROCESSING_ATTACHMENTS);
      });

  analytics_recoder().NewProcessReceivedAttachmentsEnd(
      1, location::nearby::proto::sharing::ProcessReceivedAttachmentsStatus::
             PROCESSING_STATUS_COMPLETE_PROCESSING_ATTACHMENTS);
}

TEST_F(AnalyticsRecorderTest, NewReceiveAttachmentsEnd) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::RECEIVE_ATTACHMENTS_END);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
        EXPECT_EQ(log.receive_attachments_end().session_id(), 1);
        EXPECT_EQ(log.receive_attachments_end().received_bytes(), 2);
        EXPECT_EQ(
            log.receive_attachments_end().status(),
            ::location::nearby::proto::sharing::AttachmentTransmissionStatus::
                COMPLETE_ATTACHMENT_TRANSMISSION_STATUS);
        EXPECT_EQ(log.receive_attachments_end().referrer_name(),
                  kAppPackageName);
      });

  analytics_recoder().NewReceiveAttachmentsEnd(
      1, 2,
      ::location::nearby::proto::sharing::AttachmentTransmissionStatus::
          COMPLETE_ATTACHMENT_TRANSMISSION_STATUS,
      std::string(kAppPackageName));
}

TEST_F(AnalyticsRecorderTest, NewReceiveAttachmentsStart) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::RECEIVE_ATTACHMENTS_START);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
        EXPECT_EQ(log.receive_attachments_start().session_id(), 1);
        EXPECT_EQ(log.receive_attachments_start()
                      .attachments_info()
                      .file_attachment_size(),
                  0);
      });

  analytics_recoder().NewReceiveAttachmentsStart(1, AttachmentContainer());
}

TEST_F(AnalyticsRecorderTest, NewReceiveFastInitialization) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::RECEIVE_FAST_INITIALIZATION);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
        EXPECT_EQ(log.receive_initialization()
                      .time_elapse_since_screen_unlock_millis(),
                  1);
      });

  analytics_recoder().NewReceiveFastInitialization(1);
}

TEST_F(AnalyticsRecorderTest, NewAcceptFastInitialization) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::ACCEPT_FAST_INITIALIZATION);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
      });

  analytics_recoder().NewAcceptFastInitialization();
}

TEST_F(AnalyticsRecorderTest, NewDismissFastInitialization) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::DISMISS_FAST_INITIALIZATION);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
      });

  analytics_recoder().NewDismissFastInitialization();
}

TEST_F(AnalyticsRecorderTest, NewReceiveIntroduction) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::RECEIVE_INTRODUCTION);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
        EXPECT_EQ(log.receive_introduction().session_id(), 1);
        EXPECT_EQ(log.receive_introduction().share_target_info().os_type(),
                  ::location::nearby::proto::sharing::OSType::WINDOWS);
        EXPECT_EQ(log.receive_introduction().share_target_info().device_type(),
                  ::location::nearby::proto::sharing::DeviceType::PHONE);
        EXPECT_EQ(log.receive_introduction().referrer_name(), kAppPackageName);
      });

  ShareTarget share_target;
  share_target.device_name = "share_target";
  share_target.type = ShareTargetType::kPhone;
  analytics_recoder().NewReceiveIntroduction(
      1, share_target, std::string(kAppPackageName), OSType::WINDOWS);
}

TEST_F(AnalyticsRecorderTest, NewRespondToIntroduction) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::RESPOND_TO_INTRODUCTION);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
        EXPECT_EQ(log.respond_introduction().session_id(), 1);
        EXPECT_EQ(log.respond_introduction().action(),
                  ::location::nearby::proto::sharing::ResponseToIntroduction::
                      ACCEPT_INTRODUCTION);
      });

  analytics_recoder().NewRespondToIntroduction(
      ::location::nearby::proto::sharing::ResponseToIntroduction::
          ACCEPT_INTRODUCTION,
      1);
}

TEST_F(AnalyticsRecorderTest, NewTapPrivacyNotification) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::TAP_PRIVACY_NOTIFICATION);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
      });

  analytics_recoder().NewTapPrivacyNotification();
}

TEST_F(AnalyticsRecorderTest, NewDismissPrivacyNotification) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::DISMISS_PRIVACY_NOTIFICATION);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
      });

  analytics_recoder().NewDismissPrivacyNotification();
}

TEST_F(AnalyticsRecorderTest, NewScanForShareTargetsEnd) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::SCAN_FOR_SHARE_TARGETS_END);
        EXPECT_EQ(log.event_category(), EventCategory::SENDING_EVENT);
        EXPECT_EQ(log.scan_for_share_targets_end().session_id(), 100);
      });

  analytics_recoder().NewScanForShareTargetsEnd(100);
}

TEST_F(AnalyticsRecorderTest, NewScanForShareTargetsStart) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::SCAN_FOR_SHARE_TARGETS_START);
        EXPECT_EQ(log.event_category(), EventCategory::SENDING_EVENT);
        EXPECT_EQ(log.scan_for_share_targets_start().session_id(), 3);
        EXPECT_EQ(log.scan_for_share_targets_start().status(),
                  ::location::nearby::proto::sharing::SessionStatus::
                      FAILED_SESSION_STATUS);
        EXPECT_EQ(log.scan_for_share_targets_start().flow_id(), 100);
        EXPECT_EQ(
            log.scan_for_share_targets_start().scan_type(),
            ::location::nearby::proto::sharing::ScanType::FOREGROUND_SCAN);
        EXPECT_FALSE(log.scan_for_share_targets_start().has_referrer_name());
      });

  analytics_recoder().NewScanForShareTargetsStart(
      3,
      ::location::nearby::proto::sharing::SessionStatus::FAILED_SESSION_STATUS,
      AnalyticsInformation{SendSurfaceState::kForeground}, 100, std::nullopt);
}

TEST_F(AnalyticsRecorderTest, NewSendAttachmentsEnd) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::SEND_ATTACHMENTS_END);
        EXPECT_EQ(log.event_category(), EventCategory::SENDING_EVENT);
        EXPECT_EQ(log.send_attachments_end().session_id(), 1);
        EXPECT_EQ(log.send_attachments_end().sent_bytes(), 2);
        EXPECT_EQ(log.send_attachments_end().share_target_info().os_type(),
                  ::location::nearby::proto::sharing::OSType::ANDROID);
        EXPECT_EQ(log.send_attachments_end().share_target_info().device_type(),
                  ::location::nearby::proto::sharing::DeviceType::PHONE);
        EXPECT_EQ(log.send_attachments_end().transfer_position(), 1);
        EXPECT_EQ(log.send_attachments_end().concurrent_connections(), 2);
        EXPECT_EQ(log.send_attachments_end().duration_millis(), 100);
        EXPECT_EQ(
            log.send_attachments_end().status(),
            ::location::nearby::proto::sharing::AttachmentTransmissionStatus::
                COMPLETE_ATTACHMENT_TRANSMISSION_STATUS);
        EXPECT_EQ(log.send_attachments_end().referrer_name(), kAppPackageName);
      });

  ShareTarget share_target;
  share_target.device_name = "share_target";
  share_target.type = ShareTargetType::kPhone;
  analytics_recoder().NewSendAttachmentsEnd(
      1, 2, share_target,
      ::location::nearby::proto::sharing::AttachmentTransmissionStatus::
          COMPLETE_ATTACHMENT_TRANSMISSION_STATUS,
      1, 2, 100, std::string(kAppPackageName),
      ::location::nearby::proto::sharing::ConnectionLayerStatus::
          CONNECTION_LAYER_STATUS_UNKNOWN,
      OSType::UNKNOWN_OS_TYPE);
}

TEST_F(AnalyticsRecorderTest, NewSendAttachmentsStart) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::SEND_ATTACHMENTS_START);
        EXPECT_EQ(log.event_category(), EventCategory::SENDING_EVENT);
        EXPECT_EQ(log.send_attachments_start().session_id(), 1);
        EXPECT_EQ(log.send_attachments_start()
                      .attachments_info()
                      .file_attachment_size(),
                  0);
        EXPECT_EQ(log.send_attachments_start().transfer_position(), 100);
        EXPECT_EQ(log.send_attachments_start().concurrent_connections(), 200);
      });

  analytics_recoder().NewSendAttachmentsStart(1, AttachmentContainer(), 100,
                                              200);
}

TEST_F(AnalyticsRecorderTest, NewSendFastInitialization) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::SEND_FAST_INITIALIZATION);
        EXPECT_EQ(log.event_category(), EventCategory::SENDING_EVENT);
      });

  analytics_recoder().NewSendFastInitialization();
}

TEST_F(AnalyticsRecorderTest, NewSendStart) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::SEND_START);
        EXPECT_EQ(log.event_category(), EventCategory::SENDING_EVENT);
        EXPECT_EQ(log.send_start().session_id(), 123);
        EXPECT_EQ(log.send_start().transfer_position(), 1);
        EXPECT_EQ(log.send_start().concurrent_connections(), 2);
        EXPECT_EQ(log.send_start().share_target_info().device_type(),
                  ::location::nearby::proto::sharing::DeviceType::LAPTOP);
        EXPECT_EQ(log.send_start().share_target_info().os_type(),
                  ::location::nearby::proto::sharing::OSType::UNKNOWN_OS_TYPE);
      });

  ShareTarget share_target;
  share_target.device_name = "share_target";
  share_target.type = ShareTargetType::kLaptop;
  share_target.is_known = true;
  share_target.is_incoming = true;
  analytics_recoder().NewSendStart(123, 1, 2, share_target);
}

TEST_F(AnalyticsRecorderTest, NewSendIntroductionWithRelationship) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::SEND_INTRODUCTION);
        EXPECT_EQ(log.event_category(), EventCategory::SENDING_EVENT);
        EXPECT_EQ(log.send_introduction().session_id(), 5);
        EXPECT_EQ(log.send_introduction().share_target_info().device_type(),
                  ::location::nearby::proto::sharing::DeviceType::LAPTOP);
        EXPECT_EQ(log.send_introduction().share_target_info().os_type(),
                  ::location::nearby::proto::sharing::OSType::MACOS);
        EXPECT_EQ(
            log.send_introduction().share_target_info().device_relationship(),
            ::location::nearby::proto::sharing::DeviceRelationship::IS_CONTACT);
      });

  analytics_recoder().NewSendIntroduction(
      ShareTargetType::kLaptop, 5,
      ::location::nearby::proto::sharing::DeviceRelationship::IS_CONTACT,
      OSType::MACOS);
}

TEST_F(AnalyticsRecorderTest, NewSendIntroduction) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::SEND_INTRODUCTION);
        EXPECT_EQ(log.event_category(), EventCategory::SENDING_EVENT);
        EXPECT_EQ(log.send_introduction().session_id(), 1);
        EXPECT_EQ(log.send_introduction().transfer_position(), 2);
        EXPECT_EQ(log.send_introduction().concurrent_connections(), 3);
        EXPECT_EQ(log.send_introduction().share_target_info().device_type(),
                  ::location::nearby::proto::sharing::DeviceType::LAPTOP);
        EXPECT_EQ(log.send_introduction().share_target_info().os_type(),
                  ::location::nearby::proto::sharing::OSType::UNKNOWN_OS_TYPE);
      });

  ShareTarget share_target;
  share_target.device_name = "share_target";
  share_target.type = ShareTargetType::kLaptop;
  analytics_recoder().NewSendIntroduction(1, share_target, 2, 3,
                                          OSType::UNKNOWN_OS_TYPE);
}

TEST_F(AnalyticsRecorderTest, NewSetVisibility) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::SET_VISIBILITY);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
        EXPECT_EQ(log.set_visibility().duration_millis(), 100);
        EXPECT_EQ(log.set_visibility().source_visibility(),
                  ::location::nearby::proto::sharing::Visibility::EVERYONE);
        EXPECT_EQ(
            log.set_visibility().visibility(),
            ::location::nearby::proto::sharing::Visibility::CONTACTS_ONLY);
      });

  analytics_recoder().NewSetVisibility(
      DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS, 100);
}

TEST_F(AnalyticsRecorderTest, NewDeviceSettings) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::DEVICE_SETTINGS);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
        EXPECT_EQ(log.device_settings().device_name_size(), 10);
        EXPECT_EQ(log.device_settings().visibility(),
                  ::location::nearby::proto::sharing::Visibility::EVERYONE);
        EXPECT_EQ(log.device_settings().data_usage(),
                  ::location::nearby::proto::sharing::DataUsage::WIFI_ONLY);
        EXPECT_EQ(log.device_settings().is_show_notification_enabled(), true);
      });

  AnalyticsDeviceSettings device_settings;
  device_settings.device_name_size = 10;
  device_settings.data_usage = DataUsage::WIFI_ONLY_DATA_USAGE;
  device_settings.is_fast_init_notification_enabled = true;
  device_settings.visibility = DeviceVisibility::DEVICE_VISIBILITY_EVERYONE;
  analytics_recoder().NewDeviceSettings(device_settings);
}

TEST_F(AnalyticsRecorderTest, NewFastShareServerResponse) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::FAST_SHARE_SERVER_RESPONSE);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
        EXPECT_EQ(log.fast_share_server_response().latency_millis(), 200);
        EXPECT_EQ(log.fast_share_server_response().name(),
                  ::location::nearby::proto::sharing::ServerActionName::
                      UPLOAD_CONTACTS);
        EXPECT_EQ(log.fast_share_server_response().status(),
                  ::location::nearby::proto::sharing::ServerResponseState::
                      SERVER_RESPONSE_SUCCESS);
      });

  analytics_recoder().NewFastShareServerResponse(
      ::location::nearby::proto::sharing::ServerActionName::UPLOAD_CONTACTS,
      ::location::nearby::proto::sharing::ServerResponseState::
          SERVER_RESPONSE_SUCCESS,
      200);
}

TEST_F(AnalyticsRecorderTest, NewSetDataUsage) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::SET_DATA_USAGE);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
        EXPECT_EQ(log.set_data_usage().preference(),
                  ::location::nearby::proto::sharing::DataUsage::OFFLINE);
        EXPECT_EQ(log.set_data_usage().original_preference(),
                  ::location::nearby::proto::sharing::DataUsage::WIFI_ONLY);
      });

  analytics_recoder().NewSetDataUsage(DataUsage::WIFI_ONLY_DATA_USAGE,
                                      DataUsage::OFFLINE_DATA_USAGE);
}

TEST_F(AnalyticsRecorderTest, NewAddQuickSettingsTile) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::ADD_QUICK_SETTINGS_TILE);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
      });

  analytics_recoder().NewAddQuickSettingsTile();
}

TEST_F(AnalyticsRecorderTest, NewRemoveQuickSettingsTile) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::REMOVE_QUICK_SETTINGS_TILE);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
      });

  analytics_recoder().NewRemoveQuickSettingsTile();
}

TEST_F(AnalyticsRecorderTest, NewTapQuickSettingsTile) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::TAP_QUICK_SETTINGS_TILE);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
      });

  analytics_recoder().NewTapQuickSettingsTile();
}

TEST_F(AnalyticsRecorderTest, NewToggleShowNotification) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::TOGGLE_SHOW_NOTIFICATION);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
        EXPECT_EQ(
            log.toggle_show_notification().previous_status(),
            ::location::nearby::proto::sharing::ShowNotificationStatus::SHOW);
        EXPECT_EQ(log.toggle_show_notification().current_status(),
                  ::location::nearby::proto::sharing::ShowNotificationStatus::
                      NOT_SHOW);
      });

  analytics_recoder().NewToggleShowNotification(
      ::location::nearby::proto::sharing::ShowNotificationStatus::SHOW,
      ::location::nearby::proto::sharing::ShowNotificationStatus::NOT_SHOW);
}

TEST_F(AnalyticsRecorderTest, NewSetDeviceName) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::SET_DEVICE_NAME);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
        EXPECT_EQ(log.set_device_name().device_name_size(), 16);
      });

  analytics_recoder().NewSetDeviceName(16);
}

TEST_F(AnalyticsRecorderTest, NewRequestSettingPermissions) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::REQUEST_SETTING_PERMISSIONS);
        EXPECT_EQ(log.event_category(), EventCategory::SETTINGS_EVENT);
        EXPECT_EQ(log.request_setting_permissions().permission_type(),
                  ::location::nearby::proto::sharing::PermissionRequestType::
                      PERMISSION_BLUETOOTH);
        EXPECT_EQ(
            log.request_setting_permissions().permission_request_result(),
            ::location::nearby::proto::sharing::PermissionRequestResult::
                PERMISSION_GRANTED);
      });

  analytics_recoder().NewRequestSettingPermissions(
      ::location::nearby::proto::sharing::PermissionRequestType::
          PERMISSION_BLUETOOTH,
      ::location::nearby::proto::sharing::PermissionRequestResult::
          PERMISSION_GRANTED);
}

TEST_F(AnalyticsRecorderTest, NewInstallAPKStatus) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::INSTALL_APK);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
        EXPECT_EQ(log.install_apk_status().status(0),
                  ::location::nearby::proto::sharing::InstallAPKStatus::
                      SUCCESS_INSTALLATION);
        EXPECT_EQ(
            log.install_apk_status().source(0),
            ::location::nearby::proto::sharing::ApkSource::APK_FROM_SD_CARD);
      });

  analytics_recoder().NewInstallAPKStatus(
      ::location::nearby::proto::sharing::InstallAPKStatus::
          SUCCESS_INSTALLATION,
      ::location::nearby::proto::sharing::ApkSource::APK_FROM_SD_CARD);
}

TEST_F(AnalyticsRecorderTest, NewVerifyAPKStatus) {
  EXPECT_CALL(event_logger(), Log(An<const SharingLog&>()))
      .WillOnce([](const SharingLog& log) {
        EXPECT_EQ(log.event_type(), EventType::VERIFY_APK);
        EXPECT_EQ(log.event_category(), EventCategory::RECEIVING_EVENT);
        EXPECT_EQ(
            log.verify_apk_status().status(0),
            ::location::nearby::proto::sharing::VerifyAPKStatus::INSTALLABLE);
        EXPECT_EQ(
            log.verify_apk_status().source(0),
            ::location::nearby::proto::sharing::ApkSource::APK_FROM_SD_CARD);
      });

  analytics_recoder().NewVerifyAPKStatus(
      ::location::nearby::proto::sharing::VerifyAPKStatus::INSTALLABLE,
      ::location::nearby::proto::sharing::ApkSource::APK_FROM_SD_CARD);
}

TEST_F(AnalyticsRecorderTest, GenerateID) {
  int64_t id = analytics_recoder().GenerateNextId();
  EXPECT_GT(id, 0);
  int64_t id2 = analytics_recoder().GenerateNextId();
  EXPECT_NE(id2, id);
}

}  // namespace
}  // namespace analytics
}  // namespace sharing
}  // namespace nearby
