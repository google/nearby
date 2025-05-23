// Copyright 2024 Google LLC
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

#include "sharing/incoming_share_session.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/analytics/mock_event_logger.h"
#include "internal/analytics/sharing_log_matchers.h"
#include "internal/base/file_path.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_task_runner.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment_compare.h"  // IWYU pragma: keep
#include "sharing/fake_nearby_connections_manager.h"
#include "sharing/file_attachment.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection_impl.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/proto/analytics/nearby_sharing_log.pb.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/text_attachment.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_matchers.h"
#include "sharing/wifi_credentials_attachment.h"
#include "google/protobuf/text_format.h"

namespace nearby::sharing {
namespace {

using ::absl::Seconds;
using ::location::nearby::proto::sharing::EventCategory;
using ::location::nearby::proto::sharing::EventType;
using ::location::nearby::proto::sharing::OSType;
using ::location::nearby::proto::sharing::ResponseToIntroduction;
using ::nearby::analytics::HasAction;
using ::nearby::analytics::HasCategory;
using ::nearby::analytics::HasEventType;
using ::nearby::analytics::HasSessionId;
using ::nearby::sharing::analytics::proto::SharingLog;
using ::nearby::sharing::service::proto::ConnectionResponseFrame;
using ::nearby::sharing::service::proto::FileMetadata;
using ::nearby::sharing::service::proto::Frame;
using ::nearby::sharing::service::proto::IntroductionFrame;
using ::nearby::sharing::service::proto::TextMetadata;
using ::nearby::sharing::service::proto::V1Frame;
using ::nearby::sharing::service::proto::WifiCredentials;
using ::nearby::sharing::service::proto::WifiCredentialsMetadata;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Matcher;
using ::testing::MockFunction;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

constexpr absl::string_view kEndpointId = "ABCD";

std::unique_ptr<Payload> CreateFilePayload(int64_t payload_id,
                                           FilePath file_path) {
  auto file_payload =
      std::make_unique<Payload>(InputFile(file_path.ToString()));
  file_payload->id = payload_id;
  return file_payload;
}

std::unique_ptr<Payload> CreateTextPayload(int64_t payload_id,
                                           std::string text_body) {
  auto text_payload =
      std::make_unique<Payload>(text_body.data(), text_body.size());
  text_payload->id = payload_id;
  return text_payload;
}

std::unique_ptr<Payload> CreateWifiCredentialsPayload(
    int64_t payload_id, const std::string& password, bool is_hidden) {
  WifiCredentials wifi_credentials;
  wifi_credentials.set_password(password);
  wifi_credentials.set_hidden_ssid(is_hidden);
  std::string wifi_content = wifi_credentials.SerializeAsString();
  auto wifi_payload =
      std::make_unique<Payload>(wifi_content.data(), wifi_content.size());
  wifi_payload->id = payload_id;
  return wifi_payload;
}
class IncomingShareSessionTest : public ::testing::Test {
 protected:
  IncomingShareSessionTest()
      : connection_(device_info_),
        session_(&clock_, task_runner_, &connections_manager_,
                 analytics_recorder_, std::string(kEndpointId), share_target_,
                 transfer_metadata_callback_.AsStdFunction()) {
    CHECK(
        proto2::TextFormat::ParseFromString(R"pb(
                                              file_metadata {
                                                id: 1234
                                                size: 100
                                                name: "file_name1"
                                                mime_type: "application/pdf"
                                                type: DOCUMENT
                                                parent_folder: "parent_folder1"
                                                payload_id: 9876
                                              }
                                              file_metadata {
                                                id: 1235
                                                size: 200
                                                name: "file_name2"
                                                mime_type: "image/jpeg"
                                                type: IMAGE
                                                parent_folder: "parent_folder2"
                                                payload_id: 9875
                                              }
                                              text_metadata {
                                                id: 1236
                                                size: 300
                                                text_title: "text_title1"
                                                type: URL
                                                payload_id: 9874
                                              }
                                              text_metadata {
                                                id: 1237
                                                size: 400
                                                text_title: "text_title2"
                                                type: TEXT
                                                payload_id: 9873
                                              }
                                              wifi_credentials_metadata {
                                                id: 1238
                                                ssid: "ssid1"
                                                security_type: WPA_PSK
                                                payload_id: 9872
                                              }
                                              wifi_credentials_metadata {
                                                id: 1239
                                                ssid: "ssid2"
                                                security_type: WEP
                                                payload_id: 9871
                                              }
                                            )pb",
                                            &introduction_frame_));
    payload_id1_ = introduction_frame_.file_metadata(0).payload_id();
    payload_id2_ = introduction_frame_.file_metadata(1).payload_id();
    text_payload_id1_ = introduction_frame_.text_metadata(0).payload_id();
    text_payload_id2_ = introduction_frame_.text_metadata(1).payload_id();
    wifi_payload_id1_ =
        introduction_frame_.wifi_credentials_metadata(0).payload_id();
    wifi_payload_id2_ =
        introduction_frame_.wifi_credentials_metadata(1).payload_id();
    session_.set_session_id(1234);
  }

  void TearDown() override {
    // Make sure PayloadUpdateQueue callbacks are finished.
    task_runner_.SyncWithTimeout(Seconds(1));
  }

  FakeClock clock_;
  FakeTaskRunner task_runner_{&clock_, 1};
  nearby::analytics::MockEventLogger mock_event_logger_;
  analytics::AnalyticsRecorder analytics_recorder_{/*vendor_id=*/0,
                                                   &mock_event_logger_};
  ShareTarget share_target_;
  MockFunction<void(const IncomingShareSession&, const TransferMetadata&)>
      transfer_metadata_callback_;
  FakeDeviceInfo device_info_;
  FakeNearbyConnectionsManager connections_manager_;
  NearbyConnectionImpl connection_;
  IncomingShareSession session_;
  IntroductionFrame introduction_frame_;
  int64_t payload_id1_;
  int64_t payload_id2_;
  int64_t text_payload_id1_;
  int64_t text_payload_id2_;
  int64_t wifi_payload_id1_;
  int64_t wifi_payload_id2_;
};

TEST_F(IncomingShareSessionTest, ProcessIntroductionNoSupportedPayload) {
  session_.OnConnected(&connection_);
  IntroductionFrame frame;

  EXPECT_THAT(session_.ProcessIntroduction(frame),
              Eq(TransferMetadata::Status::kUnsupportedAttachmentType));
  EXPECT_THAT(session_.attachment_container().HasAttachments(), IsFalse());
}

TEST_F(IncomingShareSessionTest, ProcessIntroductionEmptyFile) {
  session_.OnConnected(&connection_);
  IntroductionFrame frame;
  frame.mutable_file_metadata();

  EXPECT_THAT(session_.ProcessIntroduction(frame),
              Eq(TransferMetadata::Status::kUnsupportedAttachmentType));
  EXPECT_THAT(session_.attachment_container().HasAttachments(), IsFalse());
}

TEST_F(IncomingShareSessionTest, ProcessIntroductionFilesTooLarge) {
  session_.OnConnected(&connection_);
  IntroductionFrame frame;
  FileMetadata file1;
  FileMetadata file2;
  file1.set_size(std::numeric_limits<int64_t>::max());
  file2.set_size(1);
  frame.mutable_file_metadata()->Add(std::move(file1));
  frame.mutable_file_metadata()->Add(std::move(file2));

  EXPECT_THAT(session_.ProcessIntroduction(frame),
              Eq(TransferMetadata::Status::kNotEnoughSpace));
  EXPECT_THAT(session_.attachment_container().HasAttachments(), IsFalse());
}

TEST_F(IncomingShareSessionTest, ProcessIntroductionEmptyText) {
  session_.OnConnected(&connection_);
  IntroductionFrame frame;
  frame.mutable_text_metadata();

  EXPECT_THAT(session_.ProcessIntroduction(frame),
              Eq(TransferMetadata::Status::kUnsupportedAttachmentType));
  EXPECT_THAT(session_.attachment_container().HasAttachments(), IsFalse());
}

TEST_F(IncomingShareSessionTest, ProcessIntroductionSuccess) {
  session_.OnConnected(&connection_);
  FileMetadata filemeta1 = introduction_frame_.file_metadata(0);
  FileAttachment file1(filemeta1.id(), filemeta1.size(), filemeta1.name(),
                       filemeta1.mime_type(), filemeta1.type(),
                       filemeta1.parent_folder());
  FileMetadata filemeta2 = introduction_frame_.file_metadata(1);
  FileAttachment file2(filemeta2.id(), filemeta2.size(), filemeta2.name(),
                       filemeta2.mime_type(), filemeta2.type(),
                       filemeta2.parent_folder());
  TextMetadata textmeta1 = introduction_frame_.text_metadata(0);
  TextAttachment text1(textmeta1.id(), textmeta1.type(), textmeta1.text_title(),
                       textmeta1.size());
  TextMetadata textmeta2 = introduction_frame_.text_metadata(1);
  TextAttachment text2(textmeta2.id(), textmeta2.type(), textmeta2.text_title(),
                       textmeta2.size());
  WifiCredentialsMetadata wifimeta1 =
      introduction_frame_.wifi_credentials_metadata(0);
  WifiCredentialsAttachment wifi1(wifimeta1.id(), wifimeta1.ssid(),
                                  wifimeta1.security_type());
  WifiCredentialsMetadata wifimeta2 =
      introduction_frame_.wifi_credentials_metadata(1);
  WifiCredentialsAttachment wifi2(wifimeta2.id(), wifimeta2.ssid(),
                                  wifimeta2.security_type());

  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  EXPECT_THAT(session_.attachment_container().HasAttachments(), IsTrue());
  EXPECT_THAT(session_.attachment_container().GetFileAttachments(),
              UnorderedElementsAre(file1, file2));
  EXPECT_THAT(session_.attachment_container().GetTextAttachments(),
              UnorderedElementsAre(text1, text2));
  EXPECT_THAT(session_.attachment_container().GetWifiCredentialsAttachments(),
              UnorderedElementsAre(wifi1, wifi2));
  EXPECT_THAT(session_.attachment_payload_map().at(filemeta1.id()),
              Eq(filemeta1.payload_id()));
  EXPECT_THAT(session_.attachment_payload_map().at(filemeta2.id()),
              Eq(filemeta2.payload_id()));
  EXPECT_THAT(session_.attachment_payload_map().at(textmeta1.id()),
              Eq(textmeta1.payload_id()));
  EXPECT_THAT(session_.attachment_payload_map().at(textmeta2.id()),
              Eq(textmeta2.payload_id()));
  EXPECT_THAT(session_.attachment_payload_map().at(wifimeta1.id()),
              Eq(wifimeta1.payload_id()));
  EXPECT_THAT(session_.attachment_payload_map().at(wifimeta2.id()),
              Eq(wifimeta2.payload_id()));
}

TEST_F(IncomingShareSessionTest, ProcessIntroductionWithApkSuccess) {
  IntroductionFrame introduction_frame;
  CHECK(proto2::TextFormat::ParseFromString(R"pb(
                                              app_metadata {
                                                app_name: "MyApp"
                                                size: 300
                                                payload_id: 9876
                                                payload_id: 9877
                                                payload_id: 9878
                                                id: 1234
                                                file_name: "MyApp.apk"
                                                file_name: "MyApp1.apk"
                                                file_name: "MyApp2.apk"
                                                file_size: 100
                                                file_size: 100
                                                file_size: 100
                                                package_name: "com.example.myapp"
                                              }
                                            )pb",
                                            &introduction_frame));
  service::proto::AppMetadata app_metadata = introduction_frame.app_metadata(0);
  int64_t payload_id1 = app_metadata.payload_id(0);
  int64_t payload_id2 = app_metadata.payload_id(1);
  int64_t payload_id3 = app_metadata.payload_id(2);
  session_.OnConnected(&connection_);

  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame),
              Eq(std::nullopt));
  EXPECT_THAT(session_.attachment_container().HasAttachments(), IsTrue());
  // Find generated file attachments ids.
  auto it = std::find_if(std::begin(session_.attachment_payload_map()),
                         std::end(session_.attachment_payload_map()),
                         [&](auto&& p) { return p.second == payload_id1; });
  ASSERT_NE(it, std::end(session_.attachment_payload_map()));
  int64_t file_id1 = it->first;
  it = std::find_if(std::begin(session_.attachment_payload_map()),
                    std::end(session_.attachment_payload_map()),
                    [&](auto&& p) { return p.second == payload_id2; });
  ASSERT_NE(it, std::end(session_.attachment_payload_map()));
  int64_t file_id2 = it->first;
  it = std::find_if(std::begin(session_.attachment_payload_map()),
                    std::end(session_.attachment_payload_map()),
                    [&](auto&& p) { return p.second == payload_id3; });
  ASSERT_NE(it, std::end(session_.attachment_payload_map()));
  int64_t file_id3 = it->first;
  FileAttachment file1(
      file_id1, app_metadata.file_size(0), app_metadata.file_name(0),
      /*mime_type=*/"", service::proto::FileMetadata::ANDROID_APP);
  FileAttachment file2(
      file_id2, app_metadata.file_size(1), app_metadata.file_name(1),
      /*mime_type=*/"", service::proto::FileMetadata::ANDROID_APP);
  FileAttachment file3(
      file_id3, app_metadata.file_size(2), app_metadata.file_name(2),
      /*mime_type=*/"", service::proto::FileMetadata::ANDROID_APP);

  EXPECT_THAT(session_.attachment_container().GetFileAttachments(),
              UnorderedElementsAre(file1, file2, file3));
}

TEST_F(IncomingShareSessionTest,
       PayloadTransferUpdateCompleteWithWrongPayloadType) {
  connections_manager_.AcceptConnection(
      /*endpoint_info=*/{}, kEndpointId, &connection_);
  session_.OnConnected(&connection_);
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  // Set text payload for file attachment.
  connections_manager_.SetIncomingPayload(
      payload_id1_, CreateTextPayload(payload_id1_, "text1"));
  FilePath file2_path{"/usr/tmp/file2"};
  connections_manager_.SetIncomingPayload(
      payload_id2_, CreateFilePayload(payload_id2_, file2_path));
  std::string text_content1 = "text1";
  connections_manager_.SetIncomingPayload(
      text_payload_id1_, CreateTextPayload(text_payload_id1_, text_content1));
  std::string text_content2 = "text2";
  connections_manager_.SetIncomingPayload(
      text_payload_id2_, CreateTextPayload(text_payload_id2_, text_content2));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id1_,
      CreateWifiCredentialsPayload(wifi_payload_id1_, "password1", false));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id2_,
      CreateWifiCredentialsPayload(wifi_payload_id2_, "password2", true));

  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(AllOf(
          (HasCategory(EventCategory::RECEIVING_EVENT),
           HasEventType(EventType::RESPOND_TO_INTRODUCTION),
           Property(&SharingLog::respond_introduction,
                    HasAction(ResponseToIntroduction::ACCEPT_INTRODUCTION)),
           Property(&SharingLog::respond_introduction, HasSessionId(1234)))))));
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(
                  AllOf((HasCategory(EventCategory::RECEIVING_EVENT),
                         HasEventType(EventType::RECEIVE_ATTACHMENTS_START),
                         Property(&SharingLog::receive_attachments_start,
                                  HasSessionId(1234)))))));
  session_.ReadyForTransfer([]() {}, [](std::optional<V1Frame> frame) {});
  session_.AcceptTransfer([]() {});

  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id2_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          text_payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          text_payload_id2_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          wifi_payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          wifi_payload_id2_, PayloadStatus::kSuccess, 100, 100));
  EXPECT_CALL(transfer_metadata_callback_, Call(_, _)).Times(0);

  std::optional<TransferMetadata> metadata =
      session_.ProcessPayloadTransferUpdates(false);

  EXPECT_THAT(metadata.has_value(), IsTrue());
  EXPECT_THAT(*metadata,
              HasStatus(TransferMetadata::Status::kIncompletePayloads));
  // Verify that attachments are cleared out
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[0].file_path(),
      Eq(std::nullopt));
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[1].file_path(),
      Eq(std::nullopt));
  EXPECT_THAT(
      session_.attachment_container().GetTextAttachments()[0].text_body(),
      IsEmpty());
  EXPECT_THAT(
      session_.attachment_container().GetTextAttachments()[1].text_body(),
      IsEmpty());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[0]
                  .password(),
              IsEmpty());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[0]
                  .is_hidden(),
              IsFalse());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[1]
                  .password(),
              IsEmpty());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[1]
                  .is_hidden(),
              IsFalse());
  EXPECT_THAT(
      connections_manager_.connection_endpoint_info(kEndpointId).has_value(),
      IsTrue());
}

TEST_F(IncomingShareSessionTest,
       PayloadTransferUpdateCompleteWithMissingFilePayloads) {
  connections_manager_.AcceptConnection(
      /*endpoint_info=*/{}, kEndpointId, &connection_);
  session_.OnConnected(&connection_);
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FilePath file1_path{"/usr/tmp/file1"};
  connections_manager_.SetIncomingPayload(
      payload_id1_, CreateFilePayload(payload_id1_, file1_path));
  // No payload for file2.
  std::string text_content1 = "text1";
  int64_t text_payload_id1_ = introduction_frame_.text_metadata(0).payload_id();
  connections_manager_.SetIncomingPayload(
      text_payload_id1_, CreateTextPayload(text_payload_id1_, text_content1));
  std::string text_content2 = "text2";
  connections_manager_.SetIncomingPayload(
      text_payload_id2_, CreateTextPayload(text_payload_id2_, text_content2));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id1_,
      CreateWifiCredentialsPayload(wifi_payload_id1_, "password1", false));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id2_,
      CreateWifiCredentialsPayload(wifi_payload_id2_, "password2", true));

  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(AllOf(
          (HasCategory(EventCategory::RECEIVING_EVENT),
           HasEventType(EventType::RESPOND_TO_INTRODUCTION),
           Property(&SharingLog::respond_introduction,
                    HasAction(ResponseToIntroduction::ACCEPT_INTRODUCTION)),
           Property(&SharingLog::respond_introduction, HasSessionId(1234)))))));
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(
                  AllOf((HasCategory(EventCategory::RECEIVING_EVENT),
                         HasEventType(EventType::RECEIVE_ATTACHMENTS_START),
                         Property(&SharingLog::receive_attachments_start,
                                  HasSessionId(1234)))))));
  session_.ReadyForTransfer([]() {}, [](std::optional<V1Frame> frame) {});
  session_.AcceptTransfer([]() {});

  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id2_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          text_payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          text_payload_id2_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          wifi_payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          wifi_payload_id2_, PayloadStatus::kSuccess, 100, 100));
  EXPECT_CALL(transfer_metadata_callback_, Call(_, _)).Times(0);

  std::optional<TransferMetadata> metadata =
      session_.ProcessPayloadTransferUpdates(false);

  EXPECT_THAT(metadata.has_value(), IsTrue());
  EXPECT_THAT(*metadata,
              HasStatus(TransferMetadata::Status::kIncompletePayloads));
  // Verify that attachments are cleared out
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[0].file_path(),
      Eq(std::nullopt));
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[1].file_path(),
      Eq(std::nullopt));
  EXPECT_THAT(
      session_.attachment_container().GetTextAttachments()[0].text_body(),
      IsEmpty());
  EXPECT_THAT(
      session_.attachment_container().GetTextAttachments()[1].text_body(),
      IsEmpty());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[0]
                  .password(),
              IsEmpty());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[0]
                  .is_hidden(),
              IsFalse());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[1]
                  .password(),
              IsEmpty());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[1]
                  .is_hidden(),
              IsFalse());
  EXPECT_THAT(
      connections_manager_.connection_endpoint_info(kEndpointId).has_value(),
      IsTrue());
}

TEST_F(IncomingShareSessionTest,
       PayloadTransferUpdateCompleteWithMissingTextPayloads) {
  connections_manager_.AcceptConnection(
      /*endpoint_info=*/{}, kEndpointId, &connection_);
  session_.OnConnected(&connection_);
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FilePath file1_path{"/usr/tmp/file1"};
  connections_manager_.SetIncomingPayload(
      payload_id1_, CreateFilePayload(payload_id1_, file1_path));
  FilePath file2_path{"/usr/tmp/file2"};
  connections_manager_.SetIncomingPayload(
      payload_id2_, CreateFilePayload(payload_id2_, file2_path));
  std::string text_content1 = "text1";
  connections_manager_.SetIncomingPayload(
      text_payload_id1_, CreateTextPayload(text_payload_id1_, text_content1));
  // No payload for text2.
  connections_manager_.SetIncomingPayload(
      wifi_payload_id1_,
      CreateWifiCredentialsPayload(wifi_payload_id1_, "password1", false));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id2_,
      CreateWifiCredentialsPayload(wifi_payload_id2_, "password2", true));

  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(AllOf(
          (HasCategory(EventCategory::RECEIVING_EVENT),
           HasEventType(EventType::RESPOND_TO_INTRODUCTION),
           Property(&SharingLog::respond_introduction,
                    HasAction(ResponseToIntroduction::ACCEPT_INTRODUCTION)),
           Property(&SharingLog::respond_introduction, HasSessionId(1234)))))));
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(
                  AllOf((HasCategory(EventCategory::RECEIVING_EVENT),
                         HasEventType(EventType::RECEIVE_ATTACHMENTS_START),
                         Property(&SharingLog::receive_attachments_start,
                                  HasSessionId(1234)))))));
  session_.ReadyForTransfer([]() {}, [](std::optional<V1Frame> frame) {});
  session_.AcceptTransfer([]() {});
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id2_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          text_payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          text_payload_id2_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          wifi_payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          wifi_payload_id2_, PayloadStatus::kSuccess, 100, 100));
  EXPECT_CALL(transfer_metadata_callback_, Call(_, _)).Times(0);

  std::optional<TransferMetadata> metadata =
      session_.ProcessPayloadTransferUpdates(false);

  EXPECT_THAT(metadata.has_value(), IsTrue());
  EXPECT_THAT(*metadata,
              HasStatus(TransferMetadata::Status::kIncompletePayloads));
  // Verify that attachments are cleared out
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[0].file_path(),
      Eq(std::nullopt));
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[1].file_path(),
      Eq(std::nullopt));
  EXPECT_THAT(
      session_.attachment_container().GetTextAttachments()[0].text_body(),
      IsEmpty());
  EXPECT_THAT(
      session_.attachment_container().GetTextAttachments()[1].text_body(),
      IsEmpty());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[0]
                  .password(),
              IsEmpty());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[0]
                  .is_hidden(),
              IsFalse());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[1]
                  .password(),
              IsEmpty());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[1]
                  .is_hidden(),
              IsFalse());
  EXPECT_THAT(
      connections_manager_.connection_endpoint_info(kEndpointId).has_value(),
      IsTrue());
}

TEST_F(IncomingShareSessionTest,
       PayloadTransferUpdateCompleteWithMissingWifiPayloads) {
  connections_manager_.AcceptConnection(
      /*endpoint_info=*/{}, kEndpointId, &connection_);
  session_.OnConnected(&connection_);
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FilePath file1_path{"/usr/tmp/file1"};
  connections_manager_.SetIncomingPayload(
      payload_id1_, CreateFilePayload(payload_id1_, file1_path));
  FilePath file2_path{"/usr/tmp/file2"};
  connections_manager_.SetIncomingPayload(
      payload_id2_, CreateFilePayload(payload_id2_, file2_path));
  std::string text_content1 = "text1";
  connections_manager_.SetIncomingPayload(
      text_payload_id1_, CreateTextPayload(text_payload_id1_, text_content1));
  std::string text_content2 = "text2";
  connections_manager_.SetIncomingPayload(
      text_payload_id2_, CreateTextPayload(text_payload_id2_, text_content2));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id1_,
      CreateWifiCredentialsPayload(wifi_payload_id1_, "password1", false));
  // No payload for wifi2.

  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(AllOf(
          (HasCategory(EventCategory::RECEIVING_EVENT),
           HasEventType(EventType::RESPOND_TO_INTRODUCTION),
           Property(&SharingLog::respond_introduction,
                    HasAction(ResponseToIntroduction::ACCEPT_INTRODUCTION)),
           Property(&SharingLog::respond_introduction, HasSessionId(1234)))))));
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(
                  AllOf((HasCategory(EventCategory::RECEIVING_EVENT),
                         HasEventType(EventType::RECEIVE_ATTACHMENTS_START),
                         Property(&SharingLog::receive_attachments_start,
                                  HasSessionId(1234)))))));
  session_.ReadyForTransfer([]() {}, [](std::optional<V1Frame> frame) {});
  session_.AcceptTransfer([]() {});

  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id2_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          text_payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          text_payload_id2_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          wifi_payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          wifi_payload_id2_, PayloadStatus::kSuccess, 100, 100));
  EXPECT_CALL(transfer_metadata_callback_, Call(_, _)).Times(0);

  std::optional<TransferMetadata> metadata =
      session_.ProcessPayloadTransferUpdates(false);

  EXPECT_THAT(metadata.has_value(), IsTrue());
  EXPECT_THAT(*metadata,
              HasStatus(TransferMetadata::Status::kIncompletePayloads));
  // Verify that attachments are cleared out
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[0].file_path(),
      Eq(std::nullopt));
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[1].file_path(),
      Eq(std::nullopt));
  EXPECT_THAT(
      session_.attachment_container().GetTextAttachments()[0].text_body(),
      IsEmpty());
  EXPECT_THAT(
      session_.attachment_container().GetTextAttachments()[1].text_body(),
      IsEmpty());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[0]
                  .password(),
              IsEmpty());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[0]
                  .is_hidden(),
              IsFalse());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[1]
                  .password(),
              IsEmpty());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[1]
                  .is_hidden(),
              IsFalse());
  EXPECT_THAT(
      connections_manager_.connection_endpoint_info(kEndpointId).has_value(),
      IsTrue());
}

TEST_F(IncomingShareSessionTest, GetPayloadFilePaths) {
  session_.OnConnected(&connection_);
  IntroductionFrame introduction_frame;
  FileMetadata file1;
  FileMetadata file2;
  file1.set_id(23432);
  file1.set_payload_id(123);
  file1.set_size(1);
  file2.set_id(42377);
  file2.set_payload_id(456);
  file2.set_size(1);
  introduction_frame.mutable_file_metadata()->Add(std::move(file1));
  introduction_frame.mutable_file_metadata()->Add(std::move(file2));
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame),
              Eq(std::nullopt));
  FilePath file1_path{"/usr/tmp/file1"};
  int64_t payload_id1 = introduction_frame.file_metadata(0).payload_id();
  connections_manager_.SetIncomingPayload(
      payload_id1, CreateFilePayload(payload_id1, file1_path));

  FilePath file2_path{"/usr/tmp/file2"};
  int64_t payload_id2 = introduction_frame.file_metadata(1).payload_id();
  connections_manager_.SetIncomingPayload(
      payload_id2, CreateFilePayload(payload_id2, file2_path));
  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(AllOf(
          (HasCategory(EventCategory::RECEIVING_EVENT),
           HasEventType(EventType::RESPOND_TO_INTRODUCTION),
           Property(&SharingLog::respond_introduction,
                    HasAction(ResponseToIntroduction::ACCEPT_INTRODUCTION)),
           Property(&SharingLog::respond_introduction, HasSessionId(1234)))))));
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(
                  AllOf((HasCategory(EventCategory::RECEIVING_EVENT),
                         HasEventType(EventType::RECEIVE_ATTACHMENTS_START),
                         Property(&SharingLog::receive_attachments_start,
                                  HasSessionId(1234)))))));
  session_.ReadyForTransfer([]() {}, [](std::optional<V1Frame> frame) {});
  session_.AcceptTransfer([]() {});
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id1, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id2, PayloadStatus::kSuccess, 100, 100));
  std::optional<TransferMetadata> metadata =
      session_.ProcessPayloadTransferUpdates(false);

  EXPECT_THAT(metadata.has_value(), IsTrue());
  EXPECT_THAT(*metadata, HasStatus(TransferMetadata::Status::kComplete));

  std::vector<FilePath> file_paths = session_.GetPayloadFilePaths();

  EXPECT_THAT(file_paths, UnorderedElementsAre(file1_path, file2_path));
}

TEST_F(IncomingShareSessionTest, PayloadTransferUpdateCompleteWithSuccess) {
  connections_manager_.AcceptConnection(
      /*endpoint_info=*/{}, kEndpointId, &connection_);
  session_.OnConnected(&connection_);
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FilePath file1_path{"/usr/tmp/file1"};
  connections_manager_.SetIncomingPayload(
      payload_id1_, CreateFilePayload(payload_id1_, file1_path));
  FilePath file2_path{"/usr/tmp/file2"};
  connections_manager_.SetIncomingPayload(
      payload_id2_, CreateFilePayload(payload_id2_, file2_path));
  std::string text_content1 = "text1";
  connections_manager_.SetIncomingPayload(
      text_payload_id1_, CreateTextPayload(text_payload_id1_, text_content1));
  std::string text_content2 = "text2";
  connections_manager_.SetIncomingPayload(
      text_payload_id2_, CreateTextPayload(text_payload_id2_, text_content2));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id1_,
      CreateWifiCredentialsPayload(wifi_payload_id1_, "password1", false));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id2_,
      CreateWifiCredentialsPayload(wifi_payload_id2_, "password2", true));
  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(AllOf(
          (HasCategory(EventCategory::RECEIVING_EVENT),
           HasEventType(EventType::RESPOND_TO_INTRODUCTION),
           Property(&SharingLog::respond_introduction,
                    HasAction(ResponseToIntroduction::ACCEPT_INTRODUCTION)),
           Property(&SharingLog::respond_introduction, HasSessionId(1234)))))));
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(
                  AllOf((HasCategory(EventCategory::RECEIVING_EVENT),
                         HasEventType(EventType::RECEIVE_ATTACHMENTS_START),
                         Property(&SharingLog::receive_attachments_start,
                                  HasSessionId(1234)))))));
  session_.ReadyForTransfer([]() {}, [](std::optional<V1Frame> frame) {});
  session_.AcceptTransfer([]() {});
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id2_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          text_payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          text_payload_id2_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          wifi_payload_id1_, PayloadStatus::kSuccess, 100, 100));
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          wifi_payload_id2_, PayloadStatus::kSuccess, 100, 100));
  EXPECT_CALL(transfer_metadata_callback_, Call(_, _)).Times(0);

  std::optional<TransferMetadata> metadata =
      session_.ProcessPayloadTransferUpdates(false);

  EXPECT_THAT(metadata.has_value(), IsTrue());
  EXPECT_THAT(*metadata, HasStatus(TransferMetadata::Status::kComplete));
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[0].file_path(),
      Eq(file1_path));
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[1].file_path(),
      Eq(file2_path));
  EXPECT_THAT(
      session_.attachment_container().GetTextAttachments()[0].text_body(),
      Eq(text_content1));
  EXPECT_THAT(
      session_.attachment_container().GetTextAttachments()[1].text_body(),
      Eq(text_content2));
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[0]
                  .password(),
              Eq("password1"));
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[0]
                  .is_hidden(),
              IsFalse());
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[1]
                  .password(),
              Eq("password2"));
  EXPECT_THAT(session_.attachment_container()
                  .GetWifiCredentialsAttachments()[1]
                  .is_hidden(),
              IsTrue());
  EXPECT_THAT(
      connections_manager_.connection_endpoint_info(kEndpointId).has_value(),
      IsTrue());
}

TEST_F(IncomingShareSessionTest, PayloadTransferUpdateCancelled) {
  connections_manager_.AcceptConnection(
      /*endpoint_info=*/{}, kEndpointId, &connection_);
  session_.OnConnected(&connection_);
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FilePath file1_path{"/usr/tmp/file1"};
  connections_manager_.SetIncomingPayload(
      payload_id1_, CreateFilePayload(payload_id1_, file1_path));
  FilePath file2_path{"/usr/tmp/file2"};
  connections_manager_.SetIncomingPayload(
      payload_id2_, CreateFilePayload(payload_id2_, file2_path));
  std::string text_content1 = "text1";
  connections_manager_.SetIncomingPayload(
      text_payload_id1_, CreateTextPayload(text_payload_id1_, text_content1));
  std::string text_content2 = "text2";
  connections_manager_.SetIncomingPayload(
      text_payload_id2_, CreateTextPayload(text_payload_id2_, text_content2));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id1_,
      CreateWifiCredentialsPayload(wifi_payload_id1_, "password1", false));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id2_,
      CreateWifiCredentialsPayload(wifi_payload_id2_, "password2", true));
  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(AllOf(
          (HasCategory(EventCategory::RECEIVING_EVENT),
           HasEventType(EventType::RESPOND_TO_INTRODUCTION),
           Property(&SharingLog::respond_introduction,
                    HasAction(ResponseToIntroduction::ACCEPT_INTRODUCTION)),
           Property(&SharingLog::respond_introduction, HasSessionId(1234)))))));
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(
                  AllOf((HasCategory(EventCategory::RECEIVING_EVENT),
                         HasEventType(EventType::RECEIVE_ATTACHMENTS_START),
                         Property(&SharingLog::receive_attachments_start,
                                  HasSessionId(1234)))))));
  session_.ReadyForTransfer([]() {}, [](std::optional<V1Frame> frame) {});
  session_.AcceptTransfer([]() {});
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id1_, PayloadStatus::kCanceled, 100, 100));

  std::optional<TransferMetadata> metadata =
      session_.ProcessPayloadTransferUpdates(false);

  EXPECT_THAT(metadata.has_value(), IsTrue());
  EXPECT_THAT(*metadata, HasStatus(TransferMetadata::Status::kCancelled));
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[0].file_path(),
      Eq(file1_path));
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[1].file_path(),
      Eq(file2_path));
  EXPECT_THAT(
      connections_manager_.connection_endpoint_info(kEndpointId).has_value(),
      IsTrue());
}

TEST_F(IncomingShareSessionTest, PayloadTransferUpdateFailed) {
  session_.OnConnected(&connection_);
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FilePath file1_path{"/usr/tmp/file1"};
  connections_manager_.SetIncomingPayload(
      payload_id1_, CreateFilePayload(payload_id1_, file1_path));
  FilePath file2_path{"/usr/tmp/file2"};
  connections_manager_.SetIncomingPayload(
      payload_id2_, CreateFilePayload(payload_id2_, file2_path));
  std::string text_content1 = "text1";
  connections_manager_.SetIncomingPayload(
      text_payload_id1_, CreateTextPayload(text_payload_id1_, text_content1));
  std::string text_content2 = "text2";
  connections_manager_.SetIncomingPayload(
      text_payload_id2_, CreateTextPayload(text_payload_id2_, text_content2));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id1_,
      CreateWifiCredentialsPayload(wifi_payload_id1_, "password1", false));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id2_,
      CreateWifiCredentialsPayload(wifi_payload_id2_, "password2", true));
  session_.ReadyForTransfer([]() {}, [](std::optional<V1Frame> frame) {});
  session_.AcceptTransfer([]() {});
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id1_, PayloadStatus::kFailure, 100, 100));

  std::optional<TransferMetadata> metadata =
      session_.ProcessPayloadTransferUpdates(false);

  EXPECT_THAT(metadata.has_value(), IsTrue());
  EXPECT_THAT(*metadata, HasStatus(TransferMetadata::Status::kFailed));
}

TEST_F(IncomingShareSessionTest, PayloadTransferUpdateInProgress) {
  connections_manager_.AcceptConnection(
      /*endpoint_info=*/{}, kEndpointId, &connection_);
  session_.OnConnected(&connection_);
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FilePath file1_path{"/usr/tmp/file1"};
  connections_manager_.SetIncomingPayload(
      payload_id1_, CreateFilePayload(payload_id1_, file1_path));
  FilePath file2_path{"/usr/tmp/file2"};
  connections_manager_.SetIncomingPayload(
      payload_id2_, CreateFilePayload(payload_id2_, file2_path));
  std::string text_content1 = "text1";
  connections_manager_.SetIncomingPayload(
      text_payload_id1_, CreateTextPayload(text_payload_id1_, text_content1));
  std::string text_content2 = "text2";
  connections_manager_.SetIncomingPayload(
      text_payload_id2_, CreateTextPayload(text_payload_id2_, text_content2));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id1_,
      CreateWifiCredentialsPayload(wifi_payload_id1_, "password1", false));
  connections_manager_.SetIncomingPayload(
      wifi_payload_id2_,
      CreateWifiCredentialsPayload(wifi_payload_id2_, "password2", true));
  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(AllOf(
          (HasCategory(EventCategory::RECEIVING_EVENT),
           HasEventType(EventType::RESPOND_TO_INTRODUCTION),
           Property(&SharingLog::respond_introduction,
                    HasAction(ResponseToIntroduction::ACCEPT_INTRODUCTION)),
           Property(&SharingLog::respond_introduction, HasSessionId(1234)))))));
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(
                  AllOf((HasCategory(EventCategory::RECEIVING_EVENT),
                         HasEventType(EventType::RECEIVE_ATTACHMENTS_START),
                         Property(&SharingLog::receive_attachments_start,
                                  HasSessionId(1234)))))));
  session_.ReadyForTransfer([]() {}, [](std::optional<V1Frame> frame) {});
  session_.AcceptTransfer([]() {});
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id1_, PayloadStatus::kInProgress, 10, 100));

  std::optional<TransferMetadata> metadata =
      session_.ProcessPayloadTransferUpdates(false);

  EXPECT_THAT(metadata.has_value(), IsTrue());
  EXPECT_THAT(*metadata, HasStatus(TransferMetadata::Status::kInProgress));
  EXPECT_THAT(
      connections_manager_.connection_endpoint_info(kEndpointId).has_value(),
      IsTrue());
}

TEST_F(IncomingShareSessionTest, ReadyForTransferNotConnected) {
  EXPECT_THAT(
      session_.ReadyForTransfer([]() {}, [](std::optional<V1Frame> frame) {}),
      IsFalse());
}

TEST_F(IncomingShareSessionTest, ReadyForTransferNotSelfShare) {
  session_.OnConnected(&connection_);
  EXPECT_CALL(
      transfer_metadata_callback_,
      Call(_, HasStatus(TransferMetadata::Status::kAwaitingLocalConfirmation)));

  EXPECT_THAT(
      session_.ReadyForTransfer([]() {}, [](std::optional<V1Frame> frame) {}),
      IsFalse());
}

TEST_F(IncomingShareSessionTest, ReadyForTransferSelfShare) {
  ShareTarget share_target;
  share_target.for_self_share = true;
  IncomingShareSession session(&clock_, task_runner_, &connections_manager_,
                               analytics_recorder_, std::string("XYCA"),
                               share_target,
                               transfer_metadata_callback_.AsStdFunction());
  session.set_session_id(1234);
  session.OnConnected(&connection_);
  EXPECT_CALL(
      transfer_metadata_callback_,
      Call(_, HasStatus(TransferMetadata::Status::kAwaitingLocalConfirmation)))
      .Times(0);

  EXPECT_THAT(
      session.ReadyForTransfer([]() {}, [](std::optional<V1Frame> frame) {}),
      IsTrue());
}

TEST_F(IncomingShareSessionTest, ReadyForTransferTimeout) {
  session_.OnConnected(&connection_);
  EXPECT_CALL(
      transfer_metadata_callback_,
      Call(_, HasStatus(TransferMetadata::Status::kAwaitingLocalConfirmation)));
  bool accept_timeout_called = false;

  EXPECT_THAT(session_.ReadyForTransfer(
                  [&accept_timeout_called]() { accept_timeout_called = true; },
                  [](std::optional<V1Frame> frame) {}),
              IsFalse());
  clock_.FastForward(absl::Seconds(60));
  task_runner_.SyncWithTimeout(absl::Milliseconds(100));

  EXPECT_THAT(accept_timeout_called, IsTrue());
}

TEST_F(IncomingShareSessionTest, ReadyForTransferTimeoutCancelled) {
  session_.OnConnected(&connection_);
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FilePath file1_path{"/usr/tmp/file1"};
  connections_manager_.SetIncomingPayload(
      payload_id1_, CreateFilePayload(payload_id1_, file1_path));
  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(AllOf(
          (HasCategory(EventCategory::RECEIVING_EVENT),
           HasEventType(EventType::RESPOND_TO_INTRODUCTION),
           Property(&SharingLog::respond_introduction,
                    HasAction(ResponseToIntroduction::ACCEPT_INTRODUCTION)),
           Property(&SharingLog::respond_introduction, HasSessionId(1234)))))));
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(
                  AllOf((HasCategory(EventCategory::RECEIVING_EVENT),
                         HasEventType(EventType::RECEIVE_ATTACHMENTS_START),
                         Property(&SharingLog::receive_attachments_start,
                                  HasSessionId(1234)))))));

  bool accept_timeout_called = false;
  EXPECT_THAT(session_.ReadyForTransfer(
                  [&accept_timeout_called]() { accept_timeout_called = true; },
                  [](std::optional<V1Frame> frame) {}),
              IsFalse());
  session_.AcceptTransfer([]() {});
  session_.PushPayloadTransferUpdateForTest(
      std::make_unique<PayloadTransferUpdate>(
          payload_id1_, PayloadStatus::kInProgress, 10, 100));
  std::optional<TransferMetadata> metadata =
      session_.ProcessPayloadTransferUpdates(false);
  EXPECT_THAT(metadata.has_value(), IsTrue());
  EXPECT_THAT(*metadata, HasStatus(TransferMetadata::Status::kInProgress));
  clock_.FastForward(absl::Seconds(60));
  task_runner_.SyncWithTimeout(absl::Milliseconds(100));

  EXPECT_THAT(accept_timeout_called, IsFalse());
}

TEST_F(IncomingShareSessionTest, AcceptTransferNotConnected) {
  EXPECT_THAT(session_.AcceptTransfer([]() {}), IsFalse());
}

TEST_F(IncomingShareSessionTest, AcceptTransferNotReady) {
  session_.OnConnected(&connection_);
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));

  EXPECT_THAT(session_.AcceptTransfer([]() {}), IsFalse());
}

TEST_F(IncomingShareSessionTest, AcceptTransferSuccess) {
  connections_manager_.AcceptConnection(
      /*endpoint_info=*/{}, kEndpointId, &connection_);
  session_.OnConnected(&connection_);
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  EXPECT_THAT(
      session_.ReadyForTransfer([]() {}, [](std::optional<V1Frame> frame) {}),
      IsFalse());
  EXPECT_CALL(
      transfer_metadata_callback_,
      Call(_, HasStatus(TransferMetadata::Status::kAwaitingRemoteAcceptance)));
  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(AllOf(
          (HasCategory(EventCategory::RECEIVING_EVENT),
           HasEventType(EventType::RESPOND_TO_INTRODUCTION),
           Property(&SharingLog::respond_introduction,
                    HasAction(ResponseToIntroduction::ACCEPT_INTRODUCTION)),
           Property(&SharingLog::respond_introduction, HasSessionId(1234)))))));
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(
                  AllOf((HasCategory(EventCategory::RECEIVING_EVENT),
                         HasEventType(EventType::RECEIVE_ATTACHMENTS_START),
                         Property(&SharingLog::receive_attachments_start,
                                  HasSessionId(1234)))))));
  std::queue<std::vector<uint8_t>> frames_data;
  connections_manager_.set_send_payload_callback(
      [&](std::unique_ptr<Payload> payload,
          std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>
              listener) {
        frames_data.push(std::move(payload->content.bytes_payload.bytes));
      });

  EXPECT_THAT(session_.AcceptTransfer([]() {}), IsTrue());

  for (auto it : session_.attachment_payload_map()) {
    EXPECT_THAT(
        connections_manager_.GetRegisteredPayloadStatusListener(it.second)
            .lock(),
        Eq(session_.payload_tracker().lock()));
  }
  std::vector<uint8_t> frame_data = frames_data.front();
  Frame frame;
  ASSERT_TRUE(frame.ParseFromArray(frame_data.data(), frame_data.size()));
  ASSERT_EQ(frame.version(), Frame::V1);
  ASSERT_EQ(frame.v1().type(), V1Frame::RESPONSE);
  EXPECT_EQ(frame.v1().connection_response().status(),
            ConnectionResponseFrame::ACCEPT);
}

TEST_F(IncomingShareSessionTest, ProcessKeyVerificationResultSuccess) {
  session_.OnConnected(&connection_);
  session_.SetTokenForTests("1234");

  bool introduction_received = false;
  EXPECT_THAT(
      session_.ProcessKeyVerificationResult(
          PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess,
          OSType::WINDOWS,
          [&introduction_received](std::optional<IntroductionFrame>) {
            introduction_received = true;
          }),
      IsTrue());

  EXPECT_THAT(session_.self_share(), IsFalse());
  EXPECT_THAT(session_.token(), IsEmpty());
  EXPECT_THAT(session_.os_type(), Eq(OSType::WINDOWS));
  EXPECT_THAT(introduction_received, IsFalse());

  // Send Introduction frame
  nearby::sharing::service::proto::Frame frame =
      nearby::sharing::service::proto::Frame();
  frame.set_version(nearby::sharing::service::proto::Frame::V1);
  V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(service::proto::V1Frame::INTRODUCTION);
  v1frame->mutable_introduction();
  std::vector<uint8_t> data;
  data.resize(frame.ByteSizeLong());
  EXPECT_THAT(frame.SerializeToArray(data.data(), data.size()), IsTrue());
  connection_.WriteMessage(std::move(data));

  EXPECT_THAT(introduction_received, IsTrue());
}

TEST_F(IncomingShareSessionTest, ProcessKeyVerificationResultFail) {
  session_.OnConnected(&connection_);
  session_.SetTokenForTests("1234");

  bool introduction_received = false;
  EXPECT_THAT(
      session_.ProcessKeyVerificationResult(
          PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail,
          OSType::WINDOWS,
          [&introduction_received](std::optional<IntroductionFrame>) {
            introduction_received = true;
          }),
      IsFalse());

  EXPECT_THAT(session_.token(), Eq("1234"));
  EXPECT_THAT(session_.os_type(), Eq(OSType::WINDOWS));
  EXPECT_THAT(introduction_received, IsFalse());

  // Send Introduction frame
  nearby::sharing::service::proto::Frame frame =
      nearby::sharing::service::proto::Frame();
  frame.set_version(nearby::sharing::service::proto::Frame::V1);
  V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(service::proto::V1Frame::INTRODUCTION);
  v1frame->mutable_introduction();
  std::vector<uint8_t> data;
  data.resize(frame.ByteSizeLong());
  EXPECT_THAT(frame.SerializeToArray(data.data(), data.size()), IsTrue());
  connection_.WriteMessage(std::move(data));

  EXPECT_THAT(introduction_received, IsFalse());
}

TEST_F(IncomingShareSessionTest, ProcessKeyVerificationResultUnable) {
  session_.OnConnected(&connection_);
  session_.SetTokenForTests("1234");

  bool introduction_received = false;
  EXPECT_THAT(
      session_.ProcessKeyVerificationResult(
          PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable,
          OSType::WINDOWS,
          [&introduction_received](std::optional<IntroductionFrame>) {
            introduction_received = true;
          }),
      IsTrue());

  EXPECT_THAT(session_.token(), Eq("1234"));
  EXPECT_THAT(session_.os_type(), Eq(OSType::WINDOWS));
  EXPECT_THAT(introduction_received, IsFalse());

  // Send Introduction frame
  nearby::sharing::service::proto::Frame frame =
      nearby::sharing::service::proto::Frame();
  frame.set_version(nearby::sharing::service::proto::Frame::V1);
  V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(service::proto::V1Frame::INTRODUCTION);
  v1frame->mutable_introduction();
  std::vector<uint8_t> data;
  data.resize(frame.ByteSizeLong());
  EXPECT_THAT(frame.SerializeToArray(data.data(), data.size()), IsTrue());
  connection_.WriteMessage(std::move(data));

  EXPECT_THAT(introduction_received, IsTrue());
}

TEST_F(IncomingShareSessionTest, ProcessKeyVerificationResultUnknown) {
  session_.OnConnected(&connection_);
  session_.SetTokenForTests("1234");

  bool introduction_received = false;
  EXPECT_THAT(
      session_.ProcessKeyVerificationResult(
          PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnknown,
          OSType::WINDOWS,
          [&introduction_received](std::optional<IntroductionFrame>) {
            introduction_received = true;
          }),
      IsFalse());

  EXPECT_THAT(session_.token(), Eq("1234"));
  EXPECT_THAT(session_.os_type(), Eq(OSType::WINDOWS));
  EXPECT_THAT(introduction_received, IsFalse());

  // Send Introduction frame
  nearby::sharing::service::proto::Frame frame =
      nearby::sharing::service::proto::Frame();
  frame.set_version(nearby::sharing::service::proto::Frame::V1);
  V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(service::proto::V1Frame::INTRODUCTION);
  v1frame->mutable_introduction();
  std::vector<uint8_t> data;
  data.resize(frame.ByteSizeLong());
  EXPECT_THAT(frame.SerializeToArray(data.data(), data.size()), IsTrue());
  connection_.WriteMessage(std::move(data));

  EXPECT_THAT(introduction_received, IsFalse());
}

TEST_F(IncomingShareSessionTest, TryUpgradeBandwidthNotNeeded) {
  session_.OnConnected(&connection_);

  EXPECT_THAT(session_.TryUpgradeBandwidth(), IsFalse());
  EXPECT_THAT(connections_manager_.DidUpgradeBandwidth(kEndpointId), IsFalse());
}

TEST_F(IncomingShareSessionTest, TryUpgradeBandwidthNeeded) {
  IntroductionFrame introduction_frame;
  CHECK(proto2::TextFormat::ParseFromString(R"pb(
                                              file_metadata {
                                                id: 1234
                                                size: 1000000
                                                name: "file_name1"
                                                mime_type: "application/pdf"
                                                type: DOCUMENT
                                                parent_folder: "parent_folder1"
                                                payload_id: 9876
                                              }
                                              file_metadata {
                                                id: 1235
                                                size: 200
                                                name: "file_name2"
                                                mime_type: "image/jpeg"
                                                type: IMAGE
                                                parent_folder: "parent_folder2"
                                                payload_id: 9875
                                              }
                                            )pb",
                                            &introduction_frame));
  session_.OnConnected(&connection_);
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame),
              Eq(std::nullopt));

  EXPECT_THAT(session_.TryUpgradeBandwidth(), IsTrue());
  EXPECT_THAT(connections_manager_.DidUpgradeBandwidth(kEndpointId), IsTrue());
}

TEST_F(IncomingShareSessionTest, SendFailureResponseNotConnected) {
  EXPECT_CALL(transfer_metadata_callback_,
              Call(_, HasStatus(TransferMetadata::Status::kNotEnoughSpace)));

  session_.SendFailureResponse(TransferMetadata::Status::kNotEnoughSpace);
}

TEST_F(IncomingShareSessionTest, SendFailureResponseConnected) {
  connections_manager_.AcceptConnection(
      /*endpoint_info=*/{}, kEndpointId, &connection_);
  session_.OnConnected(&connection_);
  EXPECT_CALL(transfer_metadata_callback_,
              Call(_, HasStatus(TransferMetadata::Status::kNotEnoughSpace)));
  std::queue<std::vector<uint8_t>> frames_data;
  connections_manager_.set_send_payload_callback(
      [&](std::unique_ptr<Payload> payload,
          std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>
              listener) {
        frames_data.push(std::move(payload->content.bytes_payload.bytes));
      });

  session_.SendFailureResponse(TransferMetadata::Status::kNotEnoughSpace);

  std::vector<uint8_t> frame_data = frames_data.front();
  Frame frame;
  ASSERT_TRUE(frame.ParseFromArray(frame_data.data(), frame_data.size()));
  ASSERT_EQ(frame.version(), Frame::V1);
  ASSERT_EQ(frame.v1().type(), V1Frame::RESPONSE);
  EXPECT_EQ(frame.v1().connection_response().status(),
            ConnectionResponseFrame::NOT_ENOUGH_SPACE);
}

}  // namespace
}  // namespace nearby::sharing
