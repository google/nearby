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
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/attachment_container.h"
#include "sharing/fake_nearby_connection.h"
#include "sharing/fake_nearby_connections_manager.h"
#include "sharing/file_attachment.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/nearby_file_handler.h"
#include "sharing/nearby_sharing_decoder_impl.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/text_attachment.h"
#include "sharing/transfer_metadata.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby::sharing {
namespace {
using ::location::nearby::proto::sharing::OSType;
using ::nearby::sharing::service::proto::Frame;
using ::nearby::sharing::service::proto::IntroductionFrame;
using ::nearby::sharing::service::proto::ProgressUpdateFrame;
using ::nearby::sharing::service::proto::V1Frame;
using ::nearby::sharing::service::proto::WifiCredentials;
using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::MockFunction;
using ::testing::SizeIs;

constexpr absl::string_view kEndpointId = "ABCD";

class OutgoingShareSessionTest : public ::testing::Test {
 public:
  OutgoingShareSessionTest()
      : session_(fake_task_runner_, std::string(kEndpointId), share_target_,
                 [](OutgoingShareSession&, const TransferMetadata&) {}),
        text1_(nearby::sharing::service::proto::TextMetadata::URL,
               "A bit of text body", "Some text title", "text/html"),
        text2_(nearby::sharing::service::proto::TextMetadata::ADDRESS,
               "A bit of text body 2", "Some text title 2", "text/plain"),
        file1_("/usr/local/tmp/someFileName.jpg", "/usr/local/parent"),
        file2_("/usr/local/tmp/someFileName2.jpg", "/usr/local/parent2"),
        wifi1_(
            "GoogleGuest",
            nearby::sharing::service::proto::WifiCredentialsMetadata::WPA_PSK,
            "somepassword", /*is_hidden=*/true) {
    AttachmentContainer container(
        std::vector<TextAttachment>{text1_, text2_},
        std::vector<FileAttachment>{file1_},
        std::vector<WifiCredentialsAttachment>{wifi1_});
    session_.SetAttachmentContainer(std::move(container));
  }

 protected:
  FakeClock fake_clock_;
  FakeTaskRunner fake_task_runner_ {&fake_clock_, 1};
  NearbySharingDecoderImpl decoder_;
  ShareTarget share_target_;
  OutgoingShareSession session_;
  TextAttachment text1_;
  TextAttachment text2_;
  FileAttachment file1_;
  FileAttachment file2_;
  WifiCredentialsAttachment wifi1_;
};

TEST_F(OutgoingShareSessionTest, GetFilePaths) {
  OutgoingShareSession session(
      fake_task_runner_, std::string(kEndpointId), share_target_,
      [](OutgoingShareSession&, const TransferMetadata&) {});
  AttachmentContainer container(std::vector<TextAttachment>{},
                                std::vector<FileAttachment>{file1_, file2_},
                                std::vector<WifiCredentialsAttachment>{});
  session.SetAttachmentContainer(std::move(container));

  auto file_paths = session.GetFilePaths();

  ASSERT_THAT(file_paths, SizeIs(2));
  EXPECT_THAT(file_paths[0], Eq(file1_.file_path()));
  EXPECT_THAT(file_paths[1], Eq(file2_.file_path()));
}

TEST_F(OutgoingShareSessionTest, CreateTextPayloadsWithNoTextAttachments) {
  OutgoingShareSession session(
      fake_task_runner_, std::string(kEndpointId), share_target_,
      [](OutgoingShareSession&, const TransferMetadata&) {});
  session.CreateTextPayloads();
  const std::vector<Payload>& payloads = session.text_payloads();

  EXPECT_THAT(payloads, IsEmpty());
}

TEST_F(OutgoingShareSessionTest, CreateTextPayloads) {
  session_.CreateTextPayloads();
  const std::vector<Payload>& payloads = session_.text_payloads();
  auto& attachment_payload_map = session_.attachment_payload_map();

  ASSERT_THAT(payloads, SizeIs(2));
  EXPECT_THAT(payloads[0].content.type, Eq(PayloadContent::Type::kBytes));
  EXPECT_THAT(payloads[1].content.type, Eq(PayloadContent::Type::kBytes));
  EXPECT_THAT(payloads[0].content.bytes_payload.bytes,
              Eq(std::vector<uint8_t>(text1_.text_body().begin(),
                                      text1_.text_body().end())));
  EXPECT_THAT(payloads[1].content.bytes_payload.bytes,
              Eq(std::vector<uint8_t>(text2_.text_body().begin(),
                                      text2_.text_body().end())));

  ASSERT_THAT(attachment_payload_map, SizeIs(2));
  ASSERT_THAT(attachment_payload_map.contains(text1_.id()), IsTrue());
  EXPECT_THAT(attachment_payload_map.at(text1_.id()), Eq(payloads[0].id));
  ASSERT_THAT(attachment_payload_map.contains(text2_.id()), IsTrue());
  EXPECT_THAT(attachment_payload_map.at(text2_.id()), Eq(payloads[1].id));
}

TEST_F(OutgoingShareSessionTest, CreateFilePayloadsWithNoFileAttachments) {
  OutgoingShareSession session(
      fake_task_runner_, std::string(kEndpointId), share_target_,
      [](OutgoingShareSession&, const TransferMetadata&) {});

  EXPECT_THAT(
      session.CreateFilePayloads(std::vector<NearbyFileHandler::FileInfo>()),
      IsTrue());
  const std::vector<Payload>& payloads = session.file_payloads();

  EXPECT_THAT(payloads, IsEmpty());
}

TEST_F(OutgoingShareSessionTest, CreateFilePayloadsWithWrongFileInfo) {
  EXPECT_THAT(
      session_.CreateFilePayloads(std::vector<NearbyFileHandler::FileInfo>()),
      IsFalse());
  const std::vector<Payload>& payloads = session_.file_payloads();

  EXPECT_THAT(payloads, IsEmpty());
}

TEST_F(OutgoingShareSessionTest, CreateFilePayloads) {
  std::vector<NearbyFileHandler::FileInfo> file_infos;
  file_infos.push_back({
      .size = 12355L,
      .file_path = file1_.file_path().value(),
  });
  session_.CreateFilePayloads(file_infos);
  const std::vector<Payload>& payloads = session_.file_payloads();
  auto& attachment_payload_map = session_.attachment_payload_map();

  ASSERT_THAT(payloads, SizeIs(1));
  EXPECT_THAT(payloads[0].content.type, Eq(PayloadContent::Type::kFile));
  EXPECT_THAT(payloads[0].content.file_payload.size, Eq(12355L));
  EXPECT_THAT(payloads[0].content.file_payload.parent_folder,
              Eq(file1_.parent_folder()));
  EXPECT_THAT(payloads[0].content.file_payload.file.path,
              Eq(file1_.file_path()));

  EXPECT_THAT(attachment_payload_map, SizeIs(1));
  ASSERT_THAT(attachment_payload_map.contains(file1_.id()), IsTrue());
  EXPECT_THAT(attachment_payload_map.at(file1_.id()), Eq(payloads[0].id));

  EXPECT_THAT(session_.attachment_container().GetFileAttachments()[0].size(),
              Eq(12355L));
}

TEST_F(OutgoingShareSessionTest, CreateWifiPayloadsWithNoWifiAttachments) {
  OutgoingShareSession session(
      fake_task_runner_, std::string(kEndpointId), share_target_,
      [](OutgoingShareSession&, const TransferMetadata&) {});
  session.CreateWifiCredentialsPayloads();
  const std::vector<Payload>& payloads = session.file_payloads();

  EXPECT_THAT(payloads, IsEmpty());
}

TEST_F(OutgoingShareSessionTest, CreateWifiCredentialsPayloads) {
  session_.CreateWifiCredentialsPayloads();
  const std::vector<Payload>& payloads = session_.wifi_credentials_payloads();
  auto& attachment_payload_map = session_.attachment_payload_map();

  ASSERT_THAT(payloads, SizeIs(1));
  EXPECT_THAT(payloads[0].content.type, Eq(PayloadContent::Type::kBytes));
  WifiCredentials wifi_credentials;
  EXPECT_THAT(wifi_credentials.ParseFromArray(
                  payloads[0].content.bytes_payload.bytes.data(),
                  payloads[0].content.bytes_payload.bytes.size()),
              IsTrue());
  EXPECT_THAT(wifi_credentials.password(), Eq(wifi1_.password()));
  EXPECT_THAT(wifi_credentials.has_hidden_ssid(), Eq(wifi1_.is_hidden()));

  ASSERT_THAT(attachment_payload_map, SizeIs(1));
  ASSERT_THAT(attachment_payload_map.contains(wifi1_.id()), IsTrue());
  EXPECT_THAT(attachment_payload_map.at(wifi1_.id()), Eq(payloads[0].id));
}

TEST_F(OutgoingShareSessionTest, WriteIntroductionFrameWithoutPayloads) {
  EXPECT_THAT(session_.WriteIntroductionFrame(), IsFalse());
}

TEST_F(OutgoingShareSessionTest, WriteIntroductionFrameSuccess) {
  FakeNearbyConnection connection;
  session_.OnConnected(decoder_, absl::Now(), &connection);
  std::vector<NearbyFileHandler::FileInfo> file_infos;
  file_infos.push_back({
      .size = 12355L,
      .file_path = file1_.file_path().value(),
  });
  session_.CreateFilePayloads(file_infos);
  session_.CreateTextPayloads();
  session_.CreateWifiCredentialsPayloads();

  EXPECT_THAT(session_.WriteIntroductionFrame(), IsTrue());

  std::vector<uint8_t> frame_data = connection.GetWrittenData();
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
  EXPECT_THAT(intro_frame.file_metadata(0).size(), Eq(file_infos[0].size));
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

TEST_F(OutgoingShareSessionTest, SendAllPayloads) {
  std::vector<NearbyFileHandler::FileInfo> file_infos;
  file_infos.push_back({
      .size = 12355L,
      .file_path = file1_.file_path().value(),
  });
  session_.CreateFilePayloads(file_infos);
  session_.CreateTextPayloads();
  session_.CreateWifiCredentialsPayloads();
  MockFunction<void(int64_t, TransferMetadata)> transfer_metadata_callback;
  MockFunction<void(
      std::unique_ptr<Payload>,
      std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>)>
      send_payload_callback;
  FakeNearbyConnectionsManager connections_manager;
  connections_manager.set_send_payload_callback(
      send_payload_callback.AsStdFunction());
  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(Invoke(
          [this](
              std::unique_ptr<Payload> payload,
              std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = session_.attachment_payload_map().at(file1_.id());
          }))
      .WillOnce(Invoke(
          [this](
              std::unique_ptr<Payload> payload,
              std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = session_.attachment_payload_map().at(text1_.id());
          }))
      .WillOnce(Invoke(
          [this](
              std::unique_ptr<Payload> payload,
              std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = session_.attachment_payload_map().at(text2_.id());
          }));

  session_.SendAllPayloads(&fake_clock_, connections_manager,
                           transfer_metadata_callback.AsStdFunction());

  auto payload_listener = session_.payload_tracker().lock();
  EXPECT_THAT(payload_listener, IsTrue());
}

TEST_F(OutgoingShareSessionTest, InitSendPayload) {
  std::vector<NearbyFileHandler::FileInfo> file_infos;
  file_infos.push_back({
      .size = 12355L,
      .file_path = file1_.file_path().value(),
  });
  session_.CreateFilePayloads(file_infos);
  session_.CreateTextPayloads();
  session_.CreateWifiCredentialsPayloads();
  MockFunction<void(int64_t, TransferMetadata)> transfer_metadata_callback;
  MockFunction<void(
      std::unique_ptr<Payload>,
      std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>)>
      send_payload_callback;
  FakeNearbyConnectionsManager connections_manager;
  connections_manager.set_send_payload_callback(
      send_payload_callback.AsStdFunction());

  session_.InitSendPayload(&fake_clock_, connections_manager,
                           transfer_metadata_callback.AsStdFunction());

  auto payload_listener = session_.payload_tracker().lock();
  EXPECT_THAT(payload_listener, IsTrue());
}

TEST_F(OutgoingShareSessionTest, SendNextPayload) {
  std::vector<NearbyFileHandler::FileInfo> file_infos;
  file_infos.push_back({
      .size = 12355L,
      .file_path = file1_.file_path().value(),
  });
  session_.CreateFilePayloads(file_infos);
  session_.CreateTextPayloads();
  session_.CreateWifiCredentialsPayloads();
  MockFunction<void(int64_t, TransferMetadata)> transfer_metadata_callback;
  MockFunction<void(
      std::unique_ptr<Payload>,
      std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>)>
      send_payload_callback;
  FakeNearbyConnectionsManager connections_manager;
  connections_manager.set_send_payload_callback(
      send_payload_callback.AsStdFunction());

  session_.InitSendPayload(&fake_clock_, connections_manager,
                           transfer_metadata_callback.AsStdFunction());

  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(Invoke(
          [this](
              std::unique_ptr<Payload> payload,
              std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = session_.attachment_payload_map().at(file1_.id());
          }));
  session_.SendNextPayload(connections_manager);

  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(Invoke(
          [this](
              std::unique_ptr<Payload> payload,
              std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = session_.attachment_payload_map().at(text1_.id());
          }));
  session_.SendNextPayload(connections_manager);

  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(Invoke(
          [this](
              std::unique_ptr<Payload> payload,
              std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = session_.attachment_payload_map().at(text2_.id());
          }));
  session_.SendNextPayload(connections_manager);
}

TEST_F(OutgoingShareSessionTest, WriteInProgressUpdateFrameSuccess) {
  FakeNearbyConnection connection;
  session_.OnConnected(decoder_, absl::Now(), &connection);

  session_.WriteProgressUpdateFrame(true, 0.5);

  std::vector<uint8_t> frame_data = connection.GetWrittenData();
  Frame frame;
  ASSERT_THAT(frame.ParseFromArray(frame_data.data(), frame_data.size()),
              IsTrue());
  ASSERT_THAT(frame.version(), Eq(Frame::V1));
  ASSERT_THAT(frame.v1().type(), Eq(V1Frame::PROGRESS_UPDATE));
  const ProgressUpdateFrame& progress_frame = frame.v1().progress_update();
  EXPECT_THAT(progress_frame.start_transfer(), IsTrue());
  EXPECT_THAT(progress_frame.progress(), Eq(0.5));
}

TEST_F(OutgoingShareSessionTest, ProcessKeyVerificationResultFail) {
  FakeNearbyConnection connection;
  session_.OnConnected(decoder_, absl::Now(), &connection);
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
  FakeNearbyConnection connection;
  session_.OnConnected(decoder_, absl::Now(), &connection);
  session_.SetTokenForTests("1234");

  EXPECT_THAT(
      session_.ProcessKeyVerificationResult(
          PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess,
          OSType::WINDOWS),
      IsTrue());

  EXPECT_THAT(session_.token(), Eq("1234"));
  EXPECT_THAT(session_.os_type(), Eq(OSType::WINDOWS));
}

}  // namespace
}  // namespace nearby::sharing
