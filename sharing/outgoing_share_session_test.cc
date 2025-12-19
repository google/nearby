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

#include "sharing/outgoing_share_session.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
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
#include "internal/base/files.h"
#include "internal/network/url.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment_container.h"
#include "sharing/certificates/test_util.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/fake_nearby_connections_manager.h"
#include "sharing/file_attachment.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connection_impl.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/proto/analytics/nearby_sharing_log.pb.h"
#include "sharing/proto/analytics/nearby_sharing_log.proto.static_reflection.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/text_attachment.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"
#include "sharing/transfer_metadata_matchers.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby::sharing {
namespace {
using ::location::nearby::proto::sharing::EstablishConnectionStatus;
using ::location::nearby::proto::sharing::EventCategory;
using ::location::nearby::proto::sharing::EventType;
using ::location::nearby::proto::sharing::OSType;
using ::nearby::analytics::HasCategory;
using ::nearby::analytics::HasEventType;
using ::nearby::sharing::analytics::proto::SharingLog;
using ::nearby::sharing::service::proto::ConnectionResponseFrame;
using ::nearby::sharing::service::proto::Frame;
using ::nearby::sharing::service::proto::IntroductionFrame;
using ::nearby::sharing::service::proto::V1Frame;
using ::nearby::sharing::service::proto::WifiCredentials;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Matcher;
using ::testing::MockFunction;
using ::testing::Not;
using ::testing::SizeIs;
using ::testing::StrictMock;
using ::testing::proto::ProtoField;

constexpr absl::string_view kEndpointId = "ABCD";
constexpr absl::string_view kFile1Name = "someFileName.jpg";
constexpr int kFile1Size = 1234;

void CreateTempFile(absl::string_view file_name, int file_size) {
  FilePath file_path =
      Files::GetTemporaryDirectory().append(FilePath(file_name));
  std::ofstream file_stream(file_path.GetPath());
  file_stream << std::string(file_size, 'a');
  file_stream.close();
}

class OutgoingShareSessionTest : public ::testing::Test {
 public:
  OutgoingShareSessionTest()
      : session_(&fake_clock_, fake_task_runner_, &connections_manager_,
                 analytics_recorder_, std::string(kEndpointId), share_target_,
                 transfer_metadata_callback_.AsStdFunction()),
        text1_(nearby::sharing::service::proto::TextMetadata::URL,
               "A bit of text body", "Some text title", "text/html"),
        text2_(nearby::sharing::service::proto::TextMetadata::ADDRESS,
               "A bit of text body 2", "Some text title 2", "text/plain"),
        file1_(Files::GetTemporaryDirectory().append(FilePath(kFile1Name)),
               /*mime_type=*/"",
               /*parent_folder=*/"/usr/local/parent"),
        wifi1_(
            "GoogleGuest",
            nearby::sharing::service::proto::WifiCredentialsMetadata::WPA_PSK,
            "somepassword", /*is_hidden=*/true) {
    CreateTempFile(kFile1Name, kFile1Size);
  }

  std::unique_ptr<AttachmentContainer> CreateDefaultAttachmentContainer() {
    return AttachmentContainer::Builder(
               std::vector<TextAttachment>{text1_, text2_},
               std::vector<FileAttachment>{file1_},
               std::vector<WifiCredentialsAttachment>{wifi1_})
        .Build();
  }

  bool InitSendAttachments(
      std::unique_ptr<AttachmentContainer> attachment_container) {
    InSequence seq;
    EXPECT_CALL(mock_event_logger_,
                Log(Matcher<const SharingLog&>(
                    AllOf((HasCategory(EventCategory::SENDING_EVENT),
                           HasEventType(EventType::SEND_START))))));
    EXPECT_CALL(mock_event_logger_,
                Log(Matcher<const SharingLog&>(
                    AllOf((HasCategory(EventCategory::SENDING_EVENT),
                           HasEventType(EventType::DESCRIBE_ATTACHMENTS))))));
    return session_.InitiateSendAttachments(std::move(attachment_container));
  }

  void ConnectionSuccess(NearbyConnection* connection) {
    EXPECT_CALL(mock_event_logger_,
                Log(Matcher<const SharingLog&>(
                    AllOf((HasCategory(EventCategory::SENDING_EVENT),
                           HasEventType(EventType::ESTABLISH_CONNECTION))))));
    connections_manager_.set_nearby_connection(connection);
    EXPECT_CALL(transfer_metadata_callback_,
                Call(_, AllOf(HasStatus(TransferMetadata::Status::kConnecting),
                              Not(IsFinalStatus()))));
    session_.Connect(/*endpoint_info=*/{}, proto::DataUsage::ONLINE_DATA_USAGE,
                     /*disable_wifi_hotspot=*/false,
                     [](absl::string_view endpoint_id,
                        NearbyConnection* connection, Status status) {});
    EXPECT_THAT(session_.OnConnectResult(connection, Status::kSuccess),
                IsTrue());
  }

 protected:
  FakeClock fake_clock_;
  FakeTaskRunner fake_task_runner_{&fake_clock_, 1};
  nearby::analytics::MockEventLogger mock_event_logger_;
  analytics::AnalyticsRecorder analytics_recorder_{/*vendor_id=*/0,
                                                   &mock_event_logger_};
  ShareTarget share_target_;
  MockFunction<void(OutgoingShareSession&, const TransferMetadata&)>
      transfer_metadata_callback_;
  OutgoingShareSession session_;
  FakeNearbyConnectionsManager connections_manager_;
  FakeDeviceInfo device_info_;
  TextAttachment text1_;
  TextAttachment text2_;
  FileAttachment file1_;
  WifiCredentialsAttachment wifi1_;
};

TEST_F(OutgoingShareSessionTest, InitiateSendAttachmentsWithNoAttachments) {
  EXPECT_CALL(
      transfer_metadata_callback_,
      Call(_, AllOf(HasStatus(TransferMetadata::Status::kMediaUnavailable),
                    IsFinalStatus())));

  EXPECT_THAT(
      InitSendAttachments(AttachmentContainer::Builder({}, {}, {}).Build()),
      IsFalse());

  EXPECT_THAT(session_.text_payloads(), IsEmpty());
  EXPECT_THAT(session_.wifi_credentials_payloads(), IsEmpty());
  EXPECT_THAT(session_.file_payloads(), IsEmpty());
}

TEST_F(OutgoingShareSessionTest, InitiateSendAttachmentsSuccess) {
  EXPECT_THAT(InitSendAttachments(CreateDefaultAttachmentContainer()),
              IsTrue());

  const std::vector<Payload>& text_payloads = session_.text_payloads();
  ASSERT_THAT(text_payloads, SizeIs(2));
  EXPECT_THAT(text_payloads[0].content.type, Eq(PayloadContent::Type::kBytes));
  EXPECT_THAT(text_payloads[1].content.type, Eq(PayloadContent::Type::kBytes));
  EXPECT_THAT(text_payloads[0].content.bytes_payload.bytes,
              Eq(std::vector<uint8_t>(text1_.text_body().begin(),
                                      text1_.text_body().end())));
  EXPECT_THAT(text_payloads[1].content.bytes_payload.bytes,
              Eq(std::vector<uint8_t>(text2_.text_body().begin(),
                                      text2_.text_body().end())));

  const std::vector<Payload>& wifi_payloads =
      session_.wifi_credentials_payloads();

  ASSERT_THAT(wifi_payloads, SizeIs(1));
  EXPECT_THAT(wifi_payloads[0].content.type, Eq(PayloadContent::Type::kBytes));
  WifiCredentials wifi_credentials;
  EXPECT_THAT(wifi_credentials.ParseFromArray(
                  wifi_payloads[0].content.bytes_payload.bytes.data(),
                  wifi_payloads[0].content.bytes_payload.bytes.size()),
              IsTrue());
  EXPECT_THAT(wifi_credentials.password(), Eq(wifi1_.password()));
  EXPECT_THAT(wifi_credentials.has_hidden_ssid(), Eq(wifi1_.is_hidden()));

  const std::vector<Payload>& file_payloads = session_.file_payloads();
  ASSERT_THAT(file_payloads, SizeIs(1));
  EXPECT_THAT(file_payloads[0].content.type, Eq(PayloadContent::Type::kFile));
  EXPECT_THAT(file_payloads[0].content.file_payload.parent_folder,
              Eq(file1_.parent_folder()));
  EXPECT_THAT(file_payloads[0].content.file_payload.file_path,
              Eq(file1_.file_path()));


  EXPECT_THAT(session_.attachment_container().GetFileAttachments()[0].size(),
              Eq(kFile1Size));

  auto& attachment_payload_map = session_.attachment_payload_map();

  ASSERT_THAT(attachment_payload_map, SizeIs(4));
  ASSERT_THAT(attachment_payload_map.contains(text1_.id()), IsTrue());
  EXPECT_THAT(attachment_payload_map.at(text1_.id()), Eq(text_payloads[0].id));
  ASSERT_THAT(attachment_payload_map.contains(text2_.id()), IsTrue());
  EXPECT_THAT(attachment_payload_map.at(text2_.id()), Eq(text_payloads[1].id));
  ASSERT_THAT(attachment_payload_map.contains(wifi1_.id()), IsTrue());
  EXPECT_THAT(attachment_payload_map.at(wifi1_.id()), Eq(wifi_payloads[0].id));
  ASSERT_THAT(attachment_payload_map.contains(file1_.id()), IsTrue());
  EXPECT_THAT(attachment_payload_map.at(file1_.id()), Eq(file_payloads[0].id));
}

TEST_F(OutgoingShareSessionTest,
       InitiateSendAttachmentsWithNonexistentFileAttachment) {
  // Remove attachment file1.
  Files::RemoveFile(
      Files::GetTemporaryDirectory().append(FilePath(kFile1Name)));
  EXPECT_CALL(
      transfer_metadata_callback_,
      Call(_, AllOf(HasStatus(TransferMetadata::Status::kMediaUnavailable),
                    IsFinalStatus())));

  EXPECT_THAT(InitSendAttachments(CreateDefaultAttachmentContainer()),
              IsFalse());
  const std::vector<Payload>& payloads = session_.file_payloads();

  EXPECT_THAT(payloads, IsEmpty());
}

TEST_F(OutgoingShareSessionTest, ConnectNoDisableWifiHotspot) {
  std::vector<uint8_t> endpoint_info = {1, 2, 3, 4};
  // Set file size to 1MB.
  CreateTempFile(kFile1Name, 1000000);
  EXPECT_THAT(InitSendAttachments(CreateDefaultAttachmentContainer()),
              IsTrue());
  NearbyConnectionImpl nearby_connection(device_info_);
  connections_manager_.set_nearby_connection(&nearby_connection);

  EXPECT_CALL(transfer_metadata_callback_,
              Call(_, AllOf(HasStatus(TransferMetadata::Status::kConnecting),
                            Not(IsFinalStatus()))));
  session_.Connect(
      endpoint_info, nearby::sharing::proto::DataUsage::ONLINE_DATA_USAGE,
      /*disable_wifi_hotspot=*/false,
      [&nearby_connection](absl::string_view endpoint_id,
                           NearbyConnection* connection, Status status) {
        EXPECT_THAT(endpoint_id, Eq(kEndpointId));
        EXPECT_THAT(connection, Eq(&nearby_connection));
      });

  EXPECT_THAT(connections_manager_.connected_data_usage(),
              Eq(nearby::sharing::proto::DataUsage::ONLINE_DATA_USAGE));
  EXPECT_THAT(connections_manager_.transport_type(),
              Eq(TransportType::kHighQuality));
  std::optional<std::vector<uint8_t>> actual_endpoint_info =
      connections_manager_.connection_endpoint_info(kEndpointId);
  ASSERT_THAT(actual_endpoint_info.has_value(), IsTrue());
  EXPECT_THAT(actual_endpoint_info.value(), Eq(endpoint_info));
}

TEST_F(OutgoingShareSessionTest, ConnectDisableWifiHotspot) {
  std::vector<uint8_t> endpoint_info = {1, 2, 3, 4};
  // Set file size to 1MB.
  CreateTempFile(kFile1Name, 1000000);
  EXPECT_THAT(InitSendAttachments(CreateDefaultAttachmentContainer()),
              IsTrue());
  NearbyConnectionImpl nearby_connection(device_info_);
  connections_manager_.set_nearby_connection(&nearby_connection);

  EXPECT_CALL(transfer_metadata_callback_,
              Call(_, AllOf(HasStatus(TransferMetadata::Status::kConnecting),
                            Not(IsFinalStatus()))));
  session_.Connect(
      endpoint_info, nearby::sharing::proto::DataUsage::ONLINE_DATA_USAGE,
      /*disable_wifi_hotspot=*/true,
      [&nearby_connection](absl::string_view endpoint_id,
                           NearbyConnection* connection, Status status) {
        EXPECT_THAT(endpoint_id, Eq(kEndpointId));
        EXPECT_THAT(connection, Eq(&nearby_connection));
      });

  EXPECT_THAT(connections_manager_.connected_data_usage(),
              Eq(nearby::sharing::proto::DataUsage::ONLINE_DATA_USAGE));
  EXPECT_THAT(connections_manager_.transport_type(),
              Eq(TransportType::kHighQualityNonDisruptive));
  std::optional<std::vector<uint8_t>> actual_endpoint_info =
      connections_manager_.connection_endpoint_info(kEndpointId);
  ASSERT_THAT(actual_endpoint_info.has_value(), IsTrue());
  EXPECT_THAT(actual_endpoint_info.value(), Eq(endpoint_info));
}

TEST_F(OutgoingShareSessionTest, OnConnectResultSuccessLogsSessionDuration) {
  EXPECT_THAT(InitSendAttachments(CreateDefaultAttachmentContainer()),
              IsTrue());
  session_.set_session_id(1234);
  std::vector<uint8_t> endpoint_info = {1, 2, 3, 4};
  NearbyConnectionImpl nearby_connection(device_info_);
  connections_manager_.set_nearby_connection(&nearby_connection);
  EXPECT_CALL(transfer_metadata_callback_,
              Call(_, AllOf(HasStatus(TransferMetadata::Status::kConnecting),
                            Not(IsFinalStatus()))));
  session_.Connect(
      endpoint_info, nearby::sharing::proto::DataUsage::ONLINE_DATA_USAGE,
      /*disable_wifi_hotspot=*/false,
      [&nearby_connection](absl::string_view endpoint_id,
                           NearbyConnection* connection, Status status) {
        EXPECT_THAT(endpoint_id, Eq(kEndpointId));
        EXPECT_THAT(connection, Eq(&nearby_connection));
      });
  fake_clock_.FastForward(absl::Seconds(10));
  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(
          AllOf((HasCategory(EventCategory::SENDING_EVENT),
                 HasEventType(EventType::ESTABLISH_CONNECTION),
                 ProtoField<"establish_connection", "session_id">(1234),
                 ProtoField<"establish_connection", "duration_millis">(10000),
                 ProtoField<"establish_connection", "status">(
                     EstablishConnectionStatus::CONNECTION_STATUS_SUCCESS))))));

  EXPECT_THAT(session_.OnConnectResult(&nearby_connection, Status::kSuccess),
              IsTrue());
}

TEST_F(OutgoingShareSessionTest, OnConnectResultFailureLogsSessionDuration) {
  EXPECT_THAT(InitSendAttachments(CreateDefaultAttachmentContainer()),
              IsTrue());
  session_.set_session_id(1234);
  std::vector<uint8_t> endpoint_info = {1, 2, 3, 4};
  EXPECT_CALL(transfer_metadata_callback_,
              Call(_, AllOf(HasStatus(TransferMetadata::Status::kConnecting),
                            Not(IsFinalStatus()))));
  session_.Connect(endpoint_info,
                   nearby::sharing::proto::DataUsage::ONLINE_DATA_USAGE,
                   /*disable_wifi_hotspot=*/false,
                   [](absl::string_view endpoint_id,
                      NearbyConnection* connection, Status status) {});
  fake_clock_.FastForward(absl::Seconds(10));
  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(
          AllOf((HasCategory(EventCategory::SENDING_EVENT),
                 HasEventType(EventType::ESTABLISH_CONNECTION),
                 ProtoField<"establish_connection", "session_id">(1234),
                 ProtoField<"establish_connection", "duration_millis">(10000),
                 ProtoField<"establish_connection", "status">(
                     EstablishConnectionStatus::CONNECTION_STATUS_FAILURE))))));
  EXPECT_CALL(transfer_metadata_callback_,
              Call(_, AllOf(HasStatus(TransferMetadata::Status::kTimedOut),
                            IsFinalStatus())));

  EXPECT_THAT(
      session_.OnConnectResult(/*connection=*/nullptr, Status::kTimeout),
      IsFalse());
}

TEST_F(OutgoingShareSessionTest, SendIntroductionSuccess) {
  EXPECT_THAT(InitSendAttachments(CreateDefaultAttachmentContainer()),
              IsTrue());
  session_.set_session_id(1234);
  NearbyConnectionImpl connection(device_info_);
  ConnectionSuccess(&connection);
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(AllOf(
                  (HasCategory(EventCategory::SENDING_EVENT),
                   HasEventType(EventType::SEND_INTRODUCTION),
                   ProtoField<"send_introduction", "session_id">(1234))))));
  std::vector<uint8_t> frame_data;
  connections_manager_.set_send_payload_callback(
      [&](std::unique_ptr<Payload> payload,
          std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>
              listener) {
        frame_data = std::move(payload->content.bytes_payload.bytes);
      });

  EXPECT_THAT(session_.SendIntroduction([]() {}), IsTrue());

  Frame frame;
  ASSERT_THAT(frame.ParseFromArray(frame_data.data(), frame_data.size()),
              IsTrue());
  ASSERT_THAT(frame.version(), Eq(Frame::V1));
  ASSERT_THAT(frame.v1().type(), Eq(V1Frame::INTRODUCTION));
  const IntroductionFrame& intro_frame = frame.v1().introduction();
  EXPECT_THAT(intro_frame.start_transfer(), IsTrue());
  const std::vector<Payload>& text_payloads = session_.text_payloads();
  ASSERT_THAT(intro_frame.text_metadata_size(), Eq(2));
  EXPECT_THAT(intro_frame.text_metadata(0).id(), Eq(text1_.id()));
  EXPECT_THAT(intro_frame.text_metadata(0).text_title(),
              Eq(text1_.text_title()));
  EXPECT_THAT(intro_frame.text_metadata(0).type(), Eq(text1_.type()));
  EXPECT_THAT(intro_frame.text_metadata(0).size(), Eq(text1_.size()));
  EXPECT_THAT(intro_frame.text_metadata(0).payload_id(),
              Eq(text_payloads[0].id));

  EXPECT_THAT(intro_frame.text_metadata(1).id(), Eq(text2_.id()));
  EXPECT_THAT(intro_frame.text_metadata(1).text_title(),
              Eq(text2_.text_title()));
  EXPECT_THAT(intro_frame.text_metadata(1).type(), Eq(text2_.type()));
  EXPECT_THAT(intro_frame.text_metadata(1).size(), Eq(text2_.size()));
  EXPECT_THAT(intro_frame.text_metadata(1).payload_id(),
              Eq(text_payloads[1].id));

  const std::vector<Payload>& file_payloads = session_.file_payloads();
  ASSERT_THAT(intro_frame.file_metadata_size(), Eq(1));
  EXPECT_THAT(intro_frame.file_metadata(0).id(), Eq(file1_.id()));
  // File attachment size has been updated by CreateFilePayloads().
  EXPECT_THAT(intro_frame.file_metadata(0).size(), Eq(kFile1Size));
  EXPECT_THAT(intro_frame.file_metadata(0).name(), Eq(file1_.file_name()));
  EXPECT_THAT(intro_frame.file_metadata(0).payload_id(),
              Eq(file_payloads[0].id));
  EXPECT_THAT(intro_frame.file_metadata(0).type(), Eq(file1_.type()));
  EXPECT_THAT(intro_frame.file_metadata(0).mime_type(), Eq(file1_.mime_type()));

  const std::vector<Payload>& wifi_payloads =
      session_.wifi_credentials_payloads();
  ASSERT_THAT(intro_frame.wifi_credentials_metadata_size(), Eq(1));
  EXPECT_THAT(intro_frame.wifi_credentials_metadata(0).id(), Eq(wifi1_.id()));
  EXPECT_THAT(intro_frame.wifi_credentials_metadata(0).ssid(),
              Eq(wifi1_.ssid()));
  EXPECT_THAT(intro_frame.wifi_credentials_metadata(0).security_type(),
              Eq(wifi1_.security_type()));
  EXPECT_THAT(intro_frame.wifi_credentials_metadata(0).payload_id(),
              Eq(wifi_payloads[0].id));
}

TEST_F(OutgoingShareSessionTest, SendIntroductionTimeout) {
  auto container =
      AttachmentContainer::Builder(std::vector<TextAttachment>{text1_},
                                   std::vector<FileAttachment>{},
                                   std::vector<WifiCredentialsAttachment>{})
          .Build();
  EXPECT_THAT(InitSendAttachments(std::move(container)), IsTrue());
  session_.set_session_id(1234);
  NearbyConnectionImpl connection(device_info_);
  ConnectionSuccess(&connection);
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(AllOf(
                  (HasCategory(EventCategory::SENDING_EVENT),
                   HasEventType(EventType::SEND_INTRODUCTION),
                   ProtoField<"send_introduction", "session_id">(1234))))));

  bool accept_timeout_called = false;
  EXPECT_THAT(session_.SendIntroduction(
                  [&accept_timeout_called]() { accept_timeout_called = true; }),
              IsTrue());

  fake_clock_.FastForward(absl::Seconds(60));
  fake_task_runner_.SyncWithTimeout(absl::Milliseconds(100));

  EXPECT_THAT(accept_timeout_called, IsTrue());
}

TEST_F(OutgoingShareSessionTest, SendIntroductionTimeoutCancelled) {
  auto container =
      AttachmentContainer::Builder(std::vector<TextAttachment>{text1_},
                                   std::vector<FileAttachment>{},
                                   std::vector<WifiCredentialsAttachment>{})
          .Build();
  EXPECT_THAT(InitSendAttachments(std::move(container)), IsTrue());
  session_.set_session_id(1234);
  NearbyConnectionImpl connection(device_info_);
  ConnectionSuccess(&connection);
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(AllOf(
                  (HasCategory(EventCategory::SENDING_EVENT),
                   HasEventType(EventType::SEND_INTRODUCTION),
                   ProtoField<"send_introduction", "session_id">(1234))))));

  bool accept_timeout_called = false;
  EXPECT_THAT(session_.SendIntroduction(
                  [&accept_timeout_called]() { accept_timeout_called = true; }),
              IsTrue());
  ConnectionResponseFrame response;
  response.set_status(ConnectionResponseFrame::ACCEPT);
  EXPECT_CALL(transfer_metadata_callback_,
              Call(_, HasStatus(TransferMetadata::Status::kInProgress)));

  std::optional<TransferMetadata::Status> status =
      session_.HandleConnectionResponse(response);
  EXPECT_THAT(status.has_value(), IsFalse());

  fake_clock_.FastForward(absl::Seconds(60));
  fake_task_runner_.SyncWithTimeout(absl::Milliseconds(100));

  EXPECT_THAT(accept_timeout_called, IsFalse());
}

TEST_F(OutgoingShareSessionTest, AcceptTransferNotConnected) {
  EXPECT_THAT(
      session_.AcceptTransfer([](std::optional<ConnectionResponseFrame>) {}),
      IsFalse());
}

TEST_F(OutgoingShareSessionTest, AcceptTransferNotReady) {
  NearbyConnectionImpl connection(device_info_);
  session_.set_session_id(1234);
  ConnectionSuccess(&connection);

  EXPECT_THAT(
      session_.AcceptTransfer([](std::optional<ConnectionResponseFrame>) {}),
      IsFalse());
}

TEST_F(OutgoingShareSessionTest, AcceptTransferSuccess) {
  auto container =
      AttachmentContainer::Builder(std::vector<TextAttachment>{text1_},
                                   std::vector<FileAttachment>{},
                                   std::vector<WifiCredentialsAttachment>{})
          .Build();
  EXPECT_THAT(InitSendAttachments(std::move(container)), IsTrue());
  session_.set_session_id(1234);
  NearbyConnectionImpl connection(device_info_);
  ConnectionSuccess(&connection);
  EXPECT_CALL(mock_event_logger_,
              Log(Matcher<const SharingLog&>(AllOf(
                  (HasCategory(EventCategory::SENDING_EVENT),
                   HasEventType(EventType::SEND_INTRODUCTION),
                   ProtoField<"send_introduction", "session_id">(1234))))));
  EXPECT_THAT(session_.SendIntroduction([]() {}), IsTrue());
  EXPECT_CALL(
      transfer_metadata_callback_,
      Call(_, HasStatus(TransferMetadata::Status::kAwaitingRemoteAcceptance)));

  bool connection_response_received = false;
  EXPECT_THAT(
      session_.AcceptTransfer([&connection_response_received](
                                  std::optional<ConnectionResponseFrame>) {
        connection_response_received = true;
      }),
      IsTrue());

  // Send response frame
  nearby::sharing::service::proto::Frame frame =
      nearby::sharing::service::proto::Frame();
  frame.set_version(nearby::sharing::service::proto::Frame::V1);
  V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(service::proto::V1Frame::RESPONSE);
  v1frame->mutable_connection_response();
  std::vector<uint8_t> data;
  data.resize(frame.ByteSizeLong());
  EXPECT_THAT(frame.SerializeToArray(data.data(), data.size()), IsTrue());
  connection.WriteMessage(std::move(data));

  EXPECT_THAT(connection_response_received, IsTrue());
}

TEST_F(OutgoingShareSessionTest, HandleConnectionResponseEmptyResponse) {
  std::optional<TransferMetadata::Status> status =
      session_.HandleConnectionResponse(std::nullopt);

  ASSERT_THAT(status.has_value(), IsTrue());
  EXPECT_THAT(status.value(), Eq(TransferMetadata::Status::kFailed));
}

TEST_F(OutgoingShareSessionTest, HandleConnectionResponseRejectResponse) {
  ConnectionResponseFrame response;
  response.set_status(ConnectionResponseFrame::REJECT);
  std::optional<TransferMetadata::Status> status =
      session_.HandleConnectionResponse(response);

  ASSERT_THAT(status.has_value(), IsTrue());
  EXPECT_THAT(status.value(), Eq(TransferMetadata::Status::kRejected));
}

TEST_F(OutgoingShareSessionTest,
       HandleConnectionResponseNotEnoughSpaceResponse) {
  ConnectionResponseFrame response;
  response.set_status(ConnectionResponseFrame::NOT_ENOUGH_SPACE);
  std::optional<TransferMetadata::Status> status =
      session_.HandleConnectionResponse(response);

  ASSERT_THAT(status.has_value(), IsTrue());
  EXPECT_THAT(status.value(), Eq(TransferMetadata::Status::kNotEnoughSpace));
}

TEST_F(OutgoingShareSessionTest,
       HandleConnectionResponseUnsupportedTypeResponse) {
  ConnectionResponseFrame response;
  response.set_status(ConnectionResponseFrame::UNSUPPORTED_ATTACHMENT_TYPE);
  std::optional<TransferMetadata::Status> status =
      session_.HandleConnectionResponse(response);

  ASSERT_THAT(status.has_value(), IsTrue());
  EXPECT_THAT(status.value(),
              Eq(TransferMetadata::Status::kUnsupportedAttachmentType));
}

TEST_F(OutgoingShareSessionTest, HandleConnectionResponseTimeoutResponse) {
  ConnectionResponseFrame response;
  response.set_status(ConnectionResponseFrame::TIMED_OUT);
  std::optional<TransferMetadata::Status> status =
      session_.HandleConnectionResponse(response);

  ASSERT_THAT(status.has_value(), IsTrue());
  EXPECT_THAT(status.value(), Eq(TransferMetadata::Status::kTimedOut));
}

TEST_F(OutgoingShareSessionTest, HandleConnectionResponseAcceptResponse) {
  ConnectionResponseFrame response;
  response.set_status(ConnectionResponseFrame::ACCEPT);
  NearbyConnectionImpl connection(device_info_);
  session_.set_session_id(1234);
  ConnectionSuccess(&connection);
  EXPECT_CALL(transfer_metadata_callback_,
              Call(_, HasStatus(TransferMetadata::Status::kInProgress)));

  std::optional<TransferMetadata::Status> status =
      session_.HandleConnectionResponse(response);

  ASSERT_THAT(status.has_value(), IsFalse());
}

TEST_F(OutgoingShareSessionTest, SendPayloads) {
  EXPECT_THAT(InitSendAttachments(CreateDefaultAttachmentContainer()),
              IsTrue());
  session_.set_session_id(1234);
  MockFunction<void()> payload_transder_update_callback;
  StrictMock<MockFunction<void(
      std::unique_ptr<Payload>,
      std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>)>>
      send_payload_callback;
  connections_manager_.set_send_payload_callback(
      send_payload_callback.AsStdFunction());
  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(
          [this](
              std::unique_ptr<Payload> payload,
              std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = session_.attachment_payload_map().at(file1_.id());
          });
  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(AllOf(
          (HasCategory(EventCategory::SENDING_EVENT),
           HasEventType(EventType::SEND_ATTACHMENTS_START),
           ProtoField<"send_attachments_start", "session_id">(1234),
           ProtoField<"send_attachments_start", "advanced_protection_enabled">(
               false),
           ProtoField<"send_attachments_start", "advanced_protection_mismatch">(
               false))))));

  NearbyConnectionImpl connection(device_info_);
  ConnectionSuccess(&connection);

  session_.SendPayloads([](std::optional<V1Frame> frame) {},
                        payload_transder_update_callback.AsStdFunction());

  auto payload_listener = session_.payload_tracker().lock();
  EXPECT_THAT(payload_listener, IsTrue());
}

TEST_F(OutgoingShareSessionTest, SendPayloadsSetsAdvancedProtectionFlags) {
  EXPECT_THAT(InitSendAttachments(CreateDefaultAttachmentContainer()),
              IsTrue());
  session_.set_session_id(1234);
  MockFunction<void()> payload_transder_update_callback;
  StrictMock<MockFunction<void(
      std::unique_ptr<Payload>,
      std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>)>>
      send_payload_callback;
  connections_manager_.set_send_payload_callback(
      send_payload_callback.AsStdFunction());
  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(
          [this](
              std::unique_ptr<Payload> payload,
              std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = session_.attachment_payload_map().at(file1_.id());
          });
  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(AllOf(
          (HasCategory(EventCategory::SENDING_EVENT),
           HasEventType(EventType::SEND_ATTACHMENTS_START),
           ProtoField<"send_attachments_start", "session_id">(1234),
           ProtoField<"send_attachments_start", "advanced_protection_enabled">(
               true),
           ProtoField<"send_attachments_start", "advanced_protection_mismatch">(
               true))))));

  NearbyConnectionImpl connection(device_info_);
  ConnectionSuccess(&connection);

  session_.SetAdvancedProtectionStatus(/*advanced_protection_enabled=*/true,
                                       /*advanced_protection_mismatch=*/true);
  session_.SendPayloads([](std::optional<V1Frame> frame) {},
                        payload_transder_update_callback.AsStdFunction());

  auto payload_listener = session_.payload_tracker().lock();
  EXPECT_THAT(payload_listener, IsTrue());
}

TEST_F(OutgoingShareSessionTest, SendNextPayload) {
  EXPECT_THAT(InitSendAttachments(CreateDefaultAttachmentContainer()),
              IsTrue());
  session_.set_session_id(1234);
  MockFunction<void()> payload_transder_update_callback;
  StrictMock<MockFunction<void(
      std::unique_ptr<Payload>,
      std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>)>>
      send_payload_callback;
  connections_manager_.set_send_payload_callback(
      send_payload_callback.AsStdFunction());

  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(
          [this](
              std::unique_ptr<Payload> payload,
              std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = session_.attachment_payload_map().at(file1_.id());
          });
  EXPECT_CALL(
      mock_event_logger_,
      Log(Matcher<const SharingLog&>(AllOf(
          (HasCategory(EventCategory::SENDING_EVENT),
           HasEventType(EventType::SEND_ATTACHMENTS_START),
           ProtoField<"send_attachments_start", "session_id">(1234),
           ProtoField<"send_attachments_start", "advanced_protection_enabled">(
               false),
           ProtoField<"send_attachments_start", "advanced_protection_mismatch">(
               false))))));
  NearbyConnectionImpl connection(device_info_);
  ConnectionSuccess(&connection);

  session_.SendPayloads([](std::optional<V1Frame> frame) {},
                        payload_transder_update_callback.AsStdFunction());

  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(
          [this](
              std::unique_ptr<Payload> payload,
              std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = session_.attachment_payload_map().at(text1_.id());
          });
  session_.SendNextPayload();

  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(
          [this](
              std::unique_ptr<Payload> payload,
              std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = session_.attachment_payload_map().at(text2_.id());
          });
  session_.SendNextPayload();

  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(
          [this](
              std::unique_ptr<Payload> payload,
              std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = session_.attachment_payload_map().at(wifi1_.id());
          });
  session_.SendNextPayload();
}

TEST_F(OutgoingShareSessionTest, ProcessKeyVerificationResultFail) {
  NearbyConnectionImpl connection(device_info_);
  session_.set_session_id(1234);
  ConnectionSuccess(&connection);
  session_.SetTokenForTests("1234");

  EXPECT_THAT(
      session_.ProcessKeyVerificationResult(
          PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail,
          OSType::WINDOWS),
      IsFalse());

  EXPECT_THAT(session_.token(), Eq("1234"));
  EXPECT_THAT(session_.os_type(), Eq(OSType::WINDOWS));
}

TEST_F(OutgoingShareSessionTest, ProcessKeyVerificationResultSuccess) {
  NearbyConnectionImpl connection(device_info_);
  session_.set_session_id(1234);
  ConnectionSuccess(&connection);
  session_.SetTokenForTests("1234");

  EXPECT_THAT(
      session_.ProcessKeyVerificationResult(
          PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess,
          OSType::WINDOWS),
      IsTrue());

  EXPECT_THAT(session_.token(), IsEmpty());
  EXPECT_THAT(session_.os_type(), Eq(OSType::WINDOWS));
}

TEST_F(OutgoingShareSessionTest, DelayCompleteReceiverDisconnect) {
  NearbyConnectionImpl connection(device_info_);
  session_.set_session_id(1234);
  ConnectionSuccess(&connection);
  TransferMetadata complete_metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kComplete)
          .build();
  EXPECT_CALL(transfer_metadata_callback_,
              Call(_, HasStatus(TransferMetadata::Status::kInProgress)));

  session_.DelayComplete(complete_metadata);

  EXPECT_CALL(transfer_metadata_callback_,
              Call(_, HasStatus(TransferMetadata::Status::kComplete)));
  session_.OnDisconnect();
  // Fast forward to the disconnection timeout.
  fake_clock_.FastForward(absl::Seconds(60));
  fake_task_runner_.SyncWithTimeout(absl::Milliseconds(100));

  // Verify that connection is not closed since disconnect timeout has been
  // cancelled.
  EXPECT_THAT(
      connections_manager_.connection_endpoint_info(kEndpointId).has_value(),
      IsTrue());
}

TEST_F(OutgoingShareSessionTest, DelayCompleteDisconnectTimeout) {
  NearbyConnectionImpl connection(device_info_);
  session_.set_session_id(1234);
  std::vector<uint8_t> endpoint_info = {1, 2, 3, 4};
  EXPECT_CALL(transfer_metadata_callback_,
              Call(_, AllOf(HasStatus(TransferMetadata::Status::kConnecting),
                            Not(IsFinalStatus()))));
  session_.Connect(endpoint_info,
                   nearby::sharing::proto::DataUsage::ONLINE_DATA_USAGE,
                   /*disable_wifi_hotspot=*/false,
                   [&](absl::string_view endpoint_id,
                       NearbyConnection* connection, Status status) {});
  ConnectionSuccess(&connection);
  EXPECT_THAT(
      connections_manager_.connection_endpoint_info(kEndpointId).has_value(),
      IsTrue());
  TransferMetadata complete_metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kComplete)
          .build();
  EXPECT_CALL(transfer_metadata_callback_,
              Call(_, HasStatus(TransferMetadata::Status::kInProgress)));

  session_.DelayComplete(complete_metadata);
  // Fast forward to the disconnection timeout.
  fake_clock_.FastForward(absl::Seconds(60));
  fake_task_runner_.SyncWithTimeout(absl::Milliseconds(100));

  // Verify that connection is closed.
  EXPECT_THAT(
      connections_manager_.connection_endpoint_info(kEndpointId).has_value(),
      IsFalse());
}

TEST_F(OutgoingShareSessionTest, UpdateSessionForDedupWithCertificate) {
  EXPECT_FALSE(session_.certificate().has_value());
  ShareTarget share_target2{"test_update_name",      ::nearby::network::Url(),
                            ShareTargetType::kPhone,
                            /* is_incoming */ true,  "test_update_full_name",
                            /* is_known */ true,     "test_update_device_id",
                            /*for_self_share=*/true};
  share_target2.id = session_.share_target().id;
  EXPECT_THAT(session_.share_target(), Not(Eq(share_target2)));

  session_.UpdateSessionForDedup(share_target2,
                                 GetNearbyShareTestDecryptedPublicCertificate(),
                                 "test_update_endpoint_id");

  EXPECT_THAT(session_.share_target(), Eq(share_target2));
  EXPECT_TRUE(session_.certificate().has_value());
  EXPECT_THAT(session_.endpoint_id(), Eq("test_update_endpoint_id"));
  EXPECT_TRUE(session_.self_share());
}

TEST_F(OutgoingShareSessionTest, UpdateSessionForDedupWithoutCertificate) {
  session_.set_certificate(GetNearbyShareTestDecryptedPublicCertificate());
  EXPECT_TRUE(session_.certificate().has_value());
  ShareTarget share_target2{"test_update_name",      ::nearby::network::Url(),
                            ShareTargetType::kPhone,
                            /* is_incoming */ true,  "test_update_full_name",
                            /* is_known */ true,     "test_update_device_id",
                            /*for_self_share=*/true};
  share_target2.id = session_.share_target().id;
  EXPECT_THAT(session_.share_target(), Not(Eq(share_target2)));

  session_.UpdateSessionForDedup(share_target2, std::nullopt,
                                 "test_update_endpoint_id");

  EXPECT_THAT(session_.share_target(), Eq(share_target2));
  // Certificate is cleared.
  EXPECT_FALSE(session_.certificate().has_value());
  EXPECT_THAT(session_.endpoint_id(), Eq("test_update_endpoint_id"));
  EXPECT_TRUE(session_.self_share());
}

TEST_F(OutgoingShareSessionTest,
       UpdateSessionForDedupConnectedDoesNotUpdateCertAndEndpointId) {
  auto endpoint_id_org = session_.endpoint_id();
  NearbyConnectionImpl connection(device_info_);
  session_.set_session_id(1234);
  ConnectionSuccess(&connection);
  session_.set_certificate(GetNearbyShareTestDecryptedPublicCertificate());
  EXPECT_TRUE(session_.certificate().has_value());
  ShareTarget share_target2{"test_update_name",      ::nearby::network::Url(),
                            ShareTargetType::kPhone,
                            /* is_incoming */ true,  "test_update_full_name",
                            /* is_known */ true,     "test_update_device_id",
                            /*for_self_share=*/true};
  share_target2.id = session_.share_target().id;
  EXPECT_THAT(session_.share_target(), Not(Eq(share_target2)));

  session_.UpdateSessionForDedup(share_target2, std::nullopt,
                                 "test_update_endpoint_id");

  EXPECT_THAT(session_.share_target(), Eq(share_target2));
  EXPECT_TRUE(session_.certificate().has_value());
  EXPECT_THAT(session_.endpoint_id(), Eq(endpoint_id_org));
}
}  // namespace
}  // namespace nearby::sharing
