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

#include "sharing/outgoing_share_target_info.h"

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
#include "sharing/attachment_container.h"
#include "sharing/fake_nearby_connection.h"
#include "sharing/fake_nearby_connections_manager.h"
#include "sharing/file_attachment.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/nearby_file_handler.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/text_attachment.h"
#include "sharing/transfer_metadata.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby::sharing {
namespace {
using ::nearby::sharing::service::proto::Frame;
using ::nearby::sharing::service::proto::IntroductionFrame;
using ::nearby::sharing::service::proto::ProgressUpdateFrame;
using ::nearby::sharing::service::proto::V1Frame;
using ::nearby::sharing::service::proto::WifiCredentials;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::MockFunction;
using ::testing::SizeIs;
using ::testing::_;

constexpr absl::string_view kEndpointId = "ABCD";

class OutgoingShareTargetInfoTest : public ::testing::Test {
 public:
  OutgoingShareTargetInfoTest()
      : info_(std::string(kEndpointId), share_target_,
              [](OutgoingShareTargetInfo&, const TransferMetadata&) {}),
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
    info_.SetAttachmentContainer(std::move(container));
  }

 protected:
  ShareTarget share_target_;
  OutgoingShareTargetInfo info_;
  TextAttachment text1_;
  TextAttachment text2_;
  FileAttachment file1_;
  FileAttachment file2_;
  WifiCredentialsAttachment wifi1_;
};

TEST_F(OutgoingShareTargetInfoTest, GetFilePaths) {
  OutgoingShareTargetInfo info(
      std::string(kEndpointId), share_target_,
      [](OutgoingShareTargetInfo&, const TransferMetadata&) {});
  AttachmentContainer container(std::vector<TextAttachment>{},
                                std::vector<FileAttachment>{file1_, file2_},
                                std::vector<WifiCredentialsAttachment>{});
  info.SetAttachmentContainer(std::move(container));

  auto file_paths = info.GetFilePaths();

  ASSERT_THAT(file_paths, SizeIs(2));
  EXPECT_THAT(file_paths[0], Eq(file1_.file_path()));
  EXPECT_THAT(file_paths[1], Eq(file2_.file_path()));
}

TEST_F(OutgoingShareTargetInfoTest, CreateTextPayloadsWithNoTextAttachments) {
  OutgoingShareTargetInfo info(
      std::string(kEndpointId), share_target_,
      [](OutgoingShareTargetInfo&, const TransferMetadata&) {});
  info.CreateTextPayloads();
  const std::vector<Payload>& payloads = info.text_payloads();

  EXPECT_THAT(payloads, IsEmpty());
}

TEST_F(OutgoingShareTargetInfoTest, CreateTextPayloads) {
  info_.CreateTextPayloads();
  const std::vector<Payload>& payloads = info_.text_payloads();
  auto& attachment_payload_map = info_.attachment_payload_map();

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

TEST_F(OutgoingShareTargetInfoTest, CreateFilePayloadsWithNoFileAttachments) {
  OutgoingShareTargetInfo info(
      std::string(kEndpointId), share_target_,
      [](OutgoingShareTargetInfo&, const TransferMetadata&) {});

  EXPECT_THAT(
      info.CreateFilePayloads(std::vector<NearbyFileHandler::FileInfo>()),
      IsTrue());
  const std::vector<Payload>& payloads = info.file_payloads();

  EXPECT_THAT(payloads, IsEmpty());
}

TEST_F(OutgoingShareTargetInfoTest, CreateFilePayloadsWithWrongFileInfo) {
  EXPECT_THAT(
      info_.CreateFilePayloads(std::vector<NearbyFileHandler::FileInfo>()),
      IsFalse());
  const std::vector<Payload>& payloads = info_.file_payloads();

  EXPECT_THAT(payloads, IsEmpty());
}

TEST_F(OutgoingShareTargetInfoTest, CreateFilePayloads) {
  std::vector<NearbyFileHandler::FileInfo> file_infos;
  file_infos.push_back({
      .size = 12355L,
      .file_path = file1_.file_path().value(),
  });
  info_.CreateFilePayloads(file_infos);
  const std::vector<Payload>& payloads = info_.file_payloads();
  auto& attachment_payload_map = info_.attachment_payload_map();

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

  EXPECT_THAT(info_.attachment_container().GetFileAttachments()[0].size(),
              Eq(12355L));
}

TEST_F(OutgoingShareTargetInfoTest, CreateWifiPayloadsWithNoWifiAttachments) {
  OutgoingShareTargetInfo info(
      std::string(kEndpointId), share_target_,
      [](OutgoingShareTargetInfo&, const TransferMetadata&) {});
  info.CreateWifiCredentialsPayloads();
  const std::vector<Payload>& payloads = info.file_payloads();

  EXPECT_THAT(payloads, IsEmpty());
}

TEST_F(OutgoingShareTargetInfoTest, CreateWifiCredentialsPayloads) {
  info_.CreateWifiCredentialsPayloads();
  const std::vector<Payload>& payloads = info_.wifi_credentials_payloads();
  auto& attachment_payload_map = info_.attachment_payload_map();

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

TEST_F(OutgoingShareTargetInfoTest, WriteIntroductionFrameWithoutPayloads) {
  EXPECT_THAT(info_.WriteIntroductionFrame(), IsFalse());
}

TEST_F(OutgoingShareTargetInfoTest, WriteIntroductionFrameSuccess) {
  FakeNearbyConnection connection;
  info_.OnConnected(absl::Now(), &connection);
  std::vector<NearbyFileHandler::FileInfo> file_infos;
  file_infos.push_back({
      .size = 12355L,
      .file_path = file1_.file_path().value(),
  });
  info_.CreateFilePayloads(file_infos);
  info_.CreateTextPayloads();
  info_.CreateWifiCredentialsPayloads();

  EXPECT_THAT(info_.WriteIntroductionFrame(), IsTrue());

  std::vector<uint8_t> frame_data = connection.GetWrittenData();
  Frame frame;
  ASSERT_THAT(frame.ParseFromArray(frame_data.data(), frame_data.size()),
              IsTrue());
  ASSERT_THAT(frame.version(), Eq(Frame::V1));
  ASSERT_THAT(frame.v1().type(), Eq(V1Frame::INTRODUCTION));
  const IntroductionFrame& intro_frame = frame.v1().introduction();
  EXPECT_THAT(intro_frame.start_transfer(), IsTrue());
  const std::vector<Payload>& text_payloads = info_.text_payloads();
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

  const std::vector<Payload>& file_payloads = info_.file_payloads();
  ASSERT_THAT(intro_frame.file_metadata_size(), Eq(1));
  EXPECT_THAT(intro_frame.file_metadata(0).id(), Eq(file1_.id()));
  // File attachment size has been updated by CreateFilePayloads().
  EXPECT_THAT(intro_frame.file_metadata(0).size(), Eq(file_infos[0].size));
  EXPECT_THAT(intro_frame.file_metadata(0).name(), Eq(file1_.file_name()));
  EXPECT_THAT(intro_frame.file_metadata(0).payload_id(),
              Eq(file_payloads[0].id));
  EXPECT_THAT(intro_frame.file_metadata(0).type(), Eq(file1_.type()));
  EXPECT_THAT(intro_frame.file_metadata(0).mime_type(), Eq(file1_.mime_type()));

  const std::vector<Payload>& wifi_payloads = info_.wifi_credentials_payloads();
  ASSERT_THAT(intro_frame.wifi_credentials_metadata_size(), Eq(1));
  EXPECT_THAT(intro_frame.wifi_credentials_metadata(0).id(), Eq(wifi1_.id()));
  EXPECT_THAT(intro_frame.wifi_credentials_metadata(0).ssid(),
              Eq(wifi1_.ssid()));
  EXPECT_THAT(intro_frame.wifi_credentials_metadata(0).security_type(),
              Eq(wifi1_.security_type()));
  EXPECT_THAT(intro_frame.wifi_credentials_metadata(0).payload_id(),
              Eq(wifi_payloads[0].id));
}

TEST_F(OutgoingShareTargetInfoTest, SendAllPayloads) {
  std::vector<NearbyFileHandler::FileInfo> file_infos;
  file_infos.push_back({
      .size = 12355L,
      .file_path = file1_.file_path().value(),
  });
  info_.CreateFilePayloads(file_infos);
  info_.CreateTextPayloads();
  info_.CreateWifiCredentialsPayloads();
  MockFunction<void(int64_t, TransferMetadata)> transfer_metadata_callback;
  MockFunction<void(
      std::unique_ptr<Payload>,
      std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>)>
      send_payload_callback;
  FakeNearbyConnectionsManager connections_manager;
  FakeContext context;
  connections_manager.set_send_payload_callback(
      send_payload_callback.AsStdFunction());
  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(Invoke(
          [this](std::unique_ptr<Payload> payload,
             std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = info_.attachment_payload_map().at(file1_.id());
          }))
      .WillOnce(Invoke(
          [this](std::unique_ptr<Payload> payload,
             std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = info_.attachment_payload_map().at(text1_.id());
          }))
      .WillOnce(Invoke(
          [this](std::unique_ptr<Payload> payload,
             std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = info_.attachment_payload_map().at(text2_.id());
          }));

  info_.SendAllPayloads(&context, connections_manager,
                        transfer_metadata_callback.AsStdFunction());

  auto payload_listener = info_.payload_tracker().lock();
  EXPECT_THAT(payload_listener, IsTrue());
}

TEST_F(OutgoingShareTargetInfoTest, InitSendPayload) {
  std::vector<NearbyFileHandler::FileInfo> file_infos;
  file_infos.push_back({
      .size = 12355L,
      .file_path = file1_.file_path().value(),
  });
  info_.CreateFilePayloads(file_infos);
  info_.CreateTextPayloads();
  info_.CreateWifiCredentialsPayloads();
  MockFunction<void(int64_t, TransferMetadata)> transfer_metadata_callback;
  MockFunction<void(
      std::unique_ptr<Payload>,
      std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>)>
      send_payload_callback;
  FakeNearbyConnectionsManager connections_manager;
  FakeContext context;
  connections_manager.set_send_payload_callback(
      send_payload_callback.AsStdFunction());

  info_.InitSendPayload(&context, connections_manager,
                        transfer_metadata_callback.AsStdFunction());

  auto payload_listener = info_.payload_tracker().lock();
  EXPECT_THAT(payload_listener, IsTrue());
}

TEST_F(OutgoingShareTargetInfoTest, SendNextPayload) {
  std::vector<NearbyFileHandler::FileInfo> file_infos;
  file_infos.push_back({
      .size = 12355L,
      .file_path = file1_.file_path().value(),
  });
  info_.CreateFilePayloads(file_infos);
  info_.CreateTextPayloads();
  info_.CreateWifiCredentialsPayloads();
  MockFunction<void(int64_t, TransferMetadata)> transfer_metadata_callback;
  MockFunction<void(
      std::unique_ptr<Payload>,
      std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>)>
      send_payload_callback;
  FakeNearbyConnectionsManager connections_manager;
  FakeContext context;
  connections_manager.set_send_payload_callback(
      send_payload_callback.AsStdFunction());

  info_.InitSendPayload(&context, connections_manager,
                        transfer_metadata_callback.AsStdFunction());

  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(Invoke(
          [this](std::unique_ptr<Payload> payload,
             std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = info_.attachment_payload_map().at(file1_.id());
          }));
  info_.SendNextPayload(connections_manager);

  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(Invoke(
          [this](std::unique_ptr<Payload> payload,
             std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = info_.attachment_payload_map().at(text1_.id());
          }));
  info_.SendNextPayload(connections_manager);

  EXPECT_CALL(send_payload_callback, Call(_, _))
      .WillOnce(Invoke(
          [this](std::unique_ptr<Payload> payload,
             std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>) {
            payload->id = info_.attachment_payload_map().at(text2_.id());
          }));
  info_.SendNextPayload(connections_manager);
}

TEST_F(OutgoingShareTargetInfoTest, WriteInProgressUpdateFrameSuccess) {
  FakeNearbyConnection connection;
  info_.OnConnected(absl::Now(), &connection);

  info_.WriteProgressUpdateFrame(true, 0.5);

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

}  // namespace
}  // namespace nearby::sharing
