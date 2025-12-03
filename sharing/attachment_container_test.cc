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

#include "sharing/attachment_container.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/base/file_path.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/attachment_compare.h"  // IWYU pragma: keep
#include "sharing/file_attachment.h"
#include "sharing/text_attachment.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby::sharing {
namespace {

using ::location::nearby::proto::sharing::AttachmentSourceType;
using testing::Eq;
using testing::IsEmpty;
using testing::IsFalse;
using testing::IsTrue;
using testing::SizeIs;
using testing::UnorderedElementsAre;

class AttachmentContainerTest : public ::testing::Test {
 protected:
  AttachmentContainerTest()
      : text1_(/*id=*/12345L,
               nearby::sharing::service::proto::TextMetadata::URL,
               "A bit of text body", "Some text title", /*size=*/18,
               "text/html", /*batch_id=*/987654,
               AttachmentSourceType::ATTACHMENT_SOURCE_DRAG_AND_DROP),
        text2_(/*id=*/98564L,
               nearby::sharing::service::proto::TextMetadata::ADDRESS,
               "A bit of text body 2", "Some text title 2", /*size=*/20,
               "text/plain",
               /*batch_id=*/456547,
               AttachmentSourceType::ATTACHMENT_SOURCE_CONTEXT_MENU),
        file1_(/*id=*/436346L, /*size=*/100000, "someFileName", "image/jpeg",
               nearby::sharing::service::proto::FileMetadata::IMAGE,
               "/usr/local/tmp", /*batch_id=*/66657L,
               AttachmentSourceType::ATTACHMENT_SOURCE_SELECT_FILES_BUTTON),
        wifi1_(
            /*id=*/57457L, "GoogleGuest",
            nearby::sharing::service::proto::WifiCredentialsMetadata::WPA_PSK,
            "somepassword", true, /*batch_id=*/99707L,
            AttachmentSourceType::ATTACHMENT_SOURCE_PASTE) {
    file1_.set_file_path(FilePath{"/usr/local/tmp/someFileName.jpg"});
  }

  TextAttachment text1_;
  TextAttachment text2_;
  FileAttachment file1_;
  WifiCredentialsAttachment wifi1_;
};

TEST_F(AttachmentContainerTest, Constructor) {
  AttachmentContainer container = AttachmentContainer::Builder(
      std::vector<TextAttachment>{text1_, text2_},
      std::vector<FileAttachment>{file1_},
      std::vector<WifiCredentialsAttachment>{wifi1_}).Build();

  EXPECT_THAT(container.GetTextAttachments(),
              UnorderedElementsAre(text1_, text2_));
  EXPECT_THAT(container.GetFileAttachments(), UnorderedElementsAre(file1_));
  EXPECT_THAT(container.GetWifiCredentialsAttachments(),
              UnorderedElementsAre(wifi1_));
}

TEST_F(AttachmentContainerTest, AddTextAttachment) {
  AttachmentContainer container = AttachmentContainer::Builder()
                                      .AddTextAttachment(text1_)
                                      .AddTextAttachment(text2_)
                                      .Build();

  EXPECT_THAT(container.GetTextAttachments(),
              UnorderedElementsAre(text1_, text2_));
}

TEST_F(AttachmentContainerTest, AddFileAttachment) {
  AttachmentContainer container = AttachmentContainer::Builder()
                                      .AddFileAttachment(file1_)
                                      .Build();

  EXPECT_THAT(container.GetFileAttachments(), UnorderedElementsAre(file1_));
}

TEST_F(AttachmentContainerTest, AddWifiCredentialsAttachment) {
  AttachmentContainer container = AttachmentContainer::Builder()
                                      .AddWifiCredentialsAttachment(wifi1_)
                                      .Build();

  EXPECT_THAT(container.GetWifiCredentialsAttachments(),
              UnorderedElementsAre(wifi1_));
}

TEST_F(AttachmentContainerTest, GetMutableTextAttachment) {
  AttachmentContainer container = AttachmentContainer::Builder()
                                      .AddTextAttachment(text1_)
                                      .AddTextAttachment(text2_)
                                      .Build();

  EXPECT_THAT(container.GetMutableTextAttachment(0), Eq(text1_));
  EXPECT_THAT(container.GetMutableTextAttachment(1), Eq(text2_));
}

TEST_F(AttachmentContainerTest, GetMutableFileAttachment) {
  AttachmentContainer container = AttachmentContainer::Builder()
                                      .AddFileAttachment(file1_)
                                      .Build();

  EXPECT_THAT(container.GetMutableFileAttachment(0), Eq(file1_));
}

TEST_F(AttachmentContainerTest, GetMutableWifiCredentialsAttachment) {
  AttachmentContainer container = AttachmentContainer::Builder()
                                      .AddWifiCredentialsAttachment(wifi1_)
                                      .Build();

  EXPECT_THAT(container.GetMutableWifiCredentialsAttachment(0), Eq(wifi1_));
}

TEST_F(AttachmentContainerTest, AttachmentCount) {
  AttachmentContainer container =
      AttachmentContainer::Builder(
          std::vector<TextAttachment>{text1_, text2_},
          std::vector<FileAttachment>{file1_},
          std::vector<WifiCredentialsAttachment>{wifi1_})
          .Build();

  EXPECT_THAT(container.GetAttachmentCount(), Eq(4));
}

TEST_F(AttachmentContainerTest, GetTotalAttachmentsSize) {
  AttachmentContainer container =
      AttachmentContainer::Builder(std::vector<TextAttachment>{text1_, text2_},
                                   std::vector<FileAttachment>{file1_},
                                   std::vector<WifiCredentialsAttachment>{})
          .Build();

  EXPECT_THAT(container.GetTotalAttachmentsSize(), Eq(18 + 20 + 100000));
}

TEST_F(AttachmentContainerTest, HasAttachments) {
  AttachmentContainer::Builder builder = AttachmentContainer::Builder();

  EXPECT_THAT(builder.Empty(), IsTrue());

  builder.AddWifiCredentialsAttachment(wifi1_);

  EXPECT_THAT(builder.Empty(), IsFalse());
}

TEST_F(AttachmentContainerTest, ClearAttachments) {
  AttachmentContainer container =
      AttachmentContainer::Builder(
          std::vector<TextAttachment>{text1_, text2_},
          std::vector<FileAttachment>{file1_},
          std::vector<WifiCredentialsAttachment>{wifi1_})
          .Build();

  container.ClearAttachments();

  ASSERT_THAT(container.GetTextAttachments(), SizeIs(2));
  EXPECT_THAT(container.GetTextAttachments()[0].text_body(), IsEmpty());
  EXPECT_THAT(container.GetTextAttachments()[1].text_body(), IsEmpty());
  ASSERT_THAT(container.GetFileAttachments(), SizeIs(1));
  EXPECT_THAT(container.GetFileAttachments()[0].file_path(), Eq(std::nullopt));
  ASSERT_THAT(container.GetWifiCredentialsAttachments(), SizeIs(1));
  EXPECT_THAT(container.GetWifiCredentialsAttachments()[0].password(),
              IsEmpty());
  EXPECT_THAT(container.GetWifiCredentialsAttachments()[0].is_hidden(),
              IsFalse());
}

TEST_F(AttachmentContainerTest, GetStorageSize) {
  AttachmentContainer container =
      AttachmentContainer::Builder(
          std::vector<TextAttachment>{text1_, text2_},
          std::vector<FileAttachment>{file1_},
          std::vector<WifiCredentialsAttachment>{wifi1_})
          .Build();

  int64_t storage_size = container.GetStorageSize();

  EXPECT_THAT(storage_size, Eq(file1_.size()));
}

}  // namespace

}  // namespace nearby::sharing
