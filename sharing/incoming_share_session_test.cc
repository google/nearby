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

#include <cstdint>
#include <filesystem>  // NOLINT
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "sharing/attachment_compare.h"  // IWYU pragma: keep
#include "sharing/fake_nearby_connections_manager.h"
#include "sharing/file_attachment.h"
#include "sharing/internal/public/logging.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/text_attachment.h"
#include "sharing/transfer_metadata.h"
#include "sharing/wifi_credentials_attachment.h"
#include "google/protobuf/text_format.h"

namespace nearby::sharing {
namespace {

using ::nearby::sharing::service::proto::FileMetadata;
using ::nearby::sharing::service::proto::IntroductionFrame;
using ::nearby::sharing::service::proto::TextMetadata;
using ::nearby::sharing::service::proto::WifiCredentials;
using ::nearby::sharing::service::proto::WifiCredentialsMetadata;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::UnorderedElementsAre;

constexpr absl::string_view kEndpointId = "ABCD";

std::unique_ptr<Payload> CreateFilePayload(int64_t payload_id,
                                           std::filesystem::path file_path) {
  auto file_payload =
      std::make_unique<Payload>(InputFile(std::move(file_path)));
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
      : session_(std::string(kEndpointId), share_target_,
                 [](const IncomingShareSession&, const TransferMetadata&) {}) {
    NL_CHECK(
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
  }

  ShareTarget share_target_;
  IncomingShareSession session_;
  IntroductionFrame introduction_frame_;
};

TEST_F(IncomingShareSessionTest, ProcessIntroductionNoSupportedPayload) {
  IntroductionFrame frame;

  EXPECT_THAT(session_.ProcessIntroduction(frame),
              Eq(TransferMetadata::Status::kUnsupportedAttachmentType));
  EXPECT_THAT(session_.attachment_container().HasAttachments(), IsFalse());
}

TEST_F(IncomingShareSessionTest, ProcessIntroductionEmptyFile) {
  IntroductionFrame frame;
  frame.mutable_file_metadata();

  EXPECT_THAT(session_.ProcessIntroduction(frame),
              Eq(TransferMetadata::Status::kUnsupportedAttachmentType));
  EXPECT_THAT(session_.attachment_container().HasAttachments(), IsFalse());
}

TEST_F(IncomingShareSessionTest, ProcessIntroductionFilesTooLarge) {
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
  IntroductionFrame frame;
  frame.mutable_text_metadata();

  EXPECT_THAT(session_.ProcessIntroduction(frame),
              Eq(TransferMetadata::Status::kUnsupportedAttachmentType));
  EXPECT_THAT(session_.attachment_container().HasAttachments(), IsFalse());
}

TEST_F(IncomingShareSessionTest, ProcessIntroductionSuccess) {
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

TEST_F(IncomingShareSessionTest, UpdateFilePayloadPathsSuccess) {
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FakeNearbyConnectionsManager connections_manager;
  std::filesystem::path file1_path = "/usr/tmp/file1";
  int64_t payload_id1 = introduction_frame_.file_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      payload_id1, CreateFilePayload(payload_id1, file1_path));

  std::filesystem::path file2_path = "/usr/tmp/file2";
  int64_t payload_id2 = introduction_frame_.file_metadata(1).payload_id();
  connections_manager.SetIncomingPayload(
      payload_id2, CreateFilePayload(payload_id2, file2_path));

  EXPECT_THAT(session_.UpdateFilePayloadPaths(connections_manager), IsTrue());
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[0].file_path(),
      Eq(file1_path));
  EXPECT_THAT(
      session_.attachment_container().GetFileAttachments()[1].file_path(),
      Eq(file2_path));
}

TEST_F(IncomingShareSessionTest, UpdateFilePayloadPathsWrongType) {
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FakeNearbyConnectionsManager connections_manager;
  int64_t payload_id1 = introduction_frame_.file_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      payload_id1, CreateTextPayload(payload_id1, "text1"));

  std::filesystem::path file2_path = "/usr/tmp/file2";
  int64_t payload_id2 = introduction_frame_.file_metadata(1).payload_id();
  connections_manager.SetIncomingPayload(
      payload_id2, CreateFilePayload(payload_id2, file2_path));

  EXPECT_THAT(session_.UpdateFilePayloadPaths(connections_manager), IsFalse());
}

TEST_F(IncomingShareSessionTest, GetPayloadFilePaths) {
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FakeNearbyConnectionsManager connections_manager;
  std::filesystem::path file1_path = "/usr/tmp/file1";
  int64_t payload_id1 = introduction_frame_.file_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      payload_id1, CreateFilePayload(payload_id1, file1_path));

  std::filesystem::path file2_path = "/usr/tmp/file2";
  int64_t payload_id2 = introduction_frame_.file_metadata(1).payload_id();
  connections_manager.SetIncomingPayload(
      payload_id2, CreateFilePayload(payload_id2, file2_path));
  EXPECT_THAT(session_.UpdateFilePayloadPaths(connections_manager), IsTrue());

  std::vector<std::filesystem::path> file_paths =
      session_.GetPayloadFilePaths();

  EXPECT_THAT(file_paths, UnorderedElementsAre(file1_path, file2_path));
}

TEST_F(IncomingShareSessionTest, FinalizePayloadsSuccess) {
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FakeNearbyConnectionsManager connections_manager;
  std::filesystem::path file1_path = "/usr/tmp/file1";
  int64_t payload_id1 = introduction_frame_.file_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      payload_id1, CreateFilePayload(payload_id1, file1_path));

  std::filesystem::path file2_path = "/usr/tmp/file2";
  int64_t payload_id2 = introduction_frame_.file_metadata(1).payload_id();
  connections_manager.SetIncomingPayload(
      payload_id2, CreateFilePayload(payload_id2, file2_path));
  EXPECT_THAT(session_.UpdateFilePayloadPaths(connections_manager), IsTrue());

  std::string text_content1 = "text1";
  int64_t text_payload_id1 = introduction_frame_.text_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      text_payload_id1, CreateTextPayload(text_payload_id1, text_content1));

  std::string text_content2 = "text2";
  int64_t text_payload_id2 = introduction_frame_.text_metadata(1).payload_id();
  connections_manager.SetIncomingPayload(
      text_payload_id2, CreateTextPayload(text_payload_id2, text_content2));

  int64_t wifi_payload_id1 =
      introduction_frame_.wifi_credentials_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      wifi_payload_id1,
      CreateWifiCredentialsPayload(wifi_payload_id1, "password1", false));

  int64_t wifi_payload_id2 =
      introduction_frame_.wifi_credentials_metadata(1).payload_id();
  connections_manager.SetIncomingPayload(
      wifi_payload_id2,
      CreateWifiCredentialsPayload(wifi_payload_id2, "password2", true));

  EXPECT_THAT(session_.FinalizePayloads(connections_manager), IsTrue());
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
}

TEST_F(IncomingShareSessionTest, FinalizePayloadsMissingFilePayloads) {
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FakeNearbyConnectionsManager connections_manager;
  std::filesystem::path file1_path = "/usr/tmp/file1";
  int64_t payload_id1 = introduction_frame_.file_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      payload_id1, CreateFilePayload(payload_id1, file1_path));
  EXPECT_THAT(session_.UpdateFilePayloadPaths(connections_manager), IsFalse());

  std::string text_content1 = "text1";
  int64_t text_payload_id1 = introduction_frame_.text_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      text_payload_id1, CreateTextPayload(text_payload_id1, text_content1));

  std::string text_content2 = "text2";
  int64_t text_payload_id2 = introduction_frame_.text_metadata(1).payload_id();
  connections_manager.SetIncomingPayload(
      text_payload_id2, CreateTextPayload(text_payload_id2, text_content2));

  int64_t wifi_payload_id1 =
      introduction_frame_.wifi_credentials_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      wifi_payload_id1,
      CreateWifiCredentialsPayload(wifi_payload_id1, "password1", false));

  int64_t wifi_payload_id2 =
      introduction_frame_.wifi_credentials_metadata(1).payload_id();
  connections_manager.SetIncomingPayload(
      wifi_payload_id2,
      CreateWifiCredentialsPayload(wifi_payload_id2, "password2", true));

  EXPECT_THAT(session_.FinalizePayloads(connections_manager), IsFalse());

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
}

TEST_F(IncomingShareSessionTest, FinalizePayloadsMissingTextPayloads) {
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FakeNearbyConnectionsManager connections_manager;
  std::filesystem::path file1_path = "/usr/tmp/file1";
  int64_t payload_id1 = introduction_frame_.file_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      payload_id1, CreateFilePayload(payload_id1, file1_path));
  EXPECT_THAT(session_.UpdateFilePayloadPaths(connections_manager), IsFalse());

  std::filesystem::path file2_path = "/usr/tmp/file2";
  int64_t payload_id2 = introduction_frame_.file_metadata(1).payload_id();
  connections_manager.SetIncomingPayload(
      payload_id2, CreateFilePayload(payload_id2, file2_path));
  EXPECT_THAT(session_.UpdateFilePayloadPaths(connections_manager), IsTrue());

  std::string text_content1 = "text1";
  int64_t text_payload_id1 = introduction_frame_.text_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      text_payload_id1, CreateTextPayload(text_payload_id1, text_content1));

  int64_t wifi_payload_id1 =
      introduction_frame_.wifi_credentials_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      wifi_payload_id1,
      CreateWifiCredentialsPayload(wifi_payload_id1, "password1", false));

  int64_t wifi_payload_id2 =
      introduction_frame_.wifi_credentials_metadata(1).payload_id();
  connections_manager.SetIncomingPayload(
      wifi_payload_id2,
      CreateWifiCredentialsPayload(wifi_payload_id2, "password2", true));

  EXPECT_THAT(session_.FinalizePayloads(connections_manager), IsFalse());

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
}

TEST_F(IncomingShareSessionTest, FinalizePayloadsMissingWifiPayloads) {
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FakeNearbyConnectionsManager connections_manager;
  std::filesystem::path file1_path = "/usr/tmp/file1";
  int64_t payload_id1 = introduction_frame_.file_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      payload_id1, CreateFilePayload(payload_id1, file1_path));
  EXPECT_THAT(session_.UpdateFilePayloadPaths(connections_manager), IsFalse());

  std::filesystem::path file2_path = "/usr/tmp/file2";
  int64_t payload_id2 = introduction_frame_.file_metadata(1).payload_id();
  connections_manager.SetIncomingPayload(
      payload_id2, CreateFilePayload(payload_id2, file2_path));
  EXPECT_THAT(session_.UpdateFilePayloadPaths(connections_manager), IsTrue());

  std::string text_content1 = "text1";
  int64_t text_payload_id1 = introduction_frame_.text_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      text_payload_id1, CreateTextPayload(text_payload_id1, text_content1));

  std::string text_content2 = "text2";
  int64_t text_payload_id2 = introduction_frame_.text_metadata(1).payload_id();
  connections_manager.SetIncomingPayload(
      text_payload_id2, CreateTextPayload(text_payload_id2, text_content2));

  int64_t wifi_payload_id1 =
      introduction_frame_.wifi_credentials_metadata(0).payload_id();
  connections_manager.SetIncomingPayload(
      wifi_payload_id1,
      CreateWifiCredentialsPayload(wifi_payload_id1, "password1", false));

  EXPECT_THAT(session_.FinalizePayloads(connections_manager), IsFalse());

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
}

TEST_F(IncomingShareSessionTest, RegisterPayloadListenerSuccess) {
  EXPECT_THAT(session_.ProcessIntroduction(introduction_frame_),
              Eq(std::nullopt));
  FakeNearbyConnectionsManager connections_manager;
  FakeContext context;

  session_.RegisterPayloadListener(&context, connections_manager,
                                   [](int64_t, TransferMetadata) {});

  for (auto it : session_.attachment_payload_map()) {
    EXPECT_THAT(
        connections_manager.GetRegisteredPayloadStatusListener(it.second)
            .lock(),
        Eq(session_.payload_tracker().lock()));
  }
}

}  // namespace
}  // namespace nearby::sharing
