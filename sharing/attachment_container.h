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

#ifndef THIRD_PARTY_NEARBY_SHARING_ATTACHMENT_CONTAINER_H_
#define THIRD_PARTY_NEARBY_SHARING_ATTACHMENT_CONTAINER_H_

#include <cstdint>
#include <utility>
#include <vector>
#include "sharing/file_attachment.h"
#include "sharing/text_attachment.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby::sharing {

// A container for attachments.
// This class is thread-compatible (go/thread-compatible).
class AttachmentContainer {
 public:
  AttachmentContainer(
      std::vector<TextAttachment> text_attachments,
      std::vector<FileAttachment> file_attachments,
      std::vector<WifiCredentialsAttachment> wifi_credentials_attachments);
  AttachmentContainer() = default;
  AttachmentContainer(const AttachmentContainer&) = default;
  AttachmentContainer(AttachmentContainer&&) = default;
  AttachmentContainer& operator=(const AttachmentContainer&) = default;
  AttachmentContainer& operator=(AttachmentContainer&&) = default;
  ~AttachmentContainer() = default;

  const std::vector<TextAttachment>& GetTextAttachments() const {
    return text_attachments_;
  }
  const std::vector<FileAttachment>& GetFileAttachments() const {
    return file_attachments_;
  }
  const std::vector<WifiCredentialsAttachment>& GetWifiCredentialsAttachments()
      const {
    return wifi_credentials_attachments_;
  }

  void AddTextAttachment(TextAttachment text_attachment) {
    text_attachments_.push_back(std::move(text_attachment));
  }
  void AddFileAttachment(FileAttachment file_attachment) {
    file_attachments_.push_back(std::move(file_attachment));
  }
  void AddWifiCredentialsAttachment(
      WifiCredentialsAttachment wifi_credentials_attachment) {
    wifi_credentials_attachments_.push_back(
        std::move(wifi_credentials_attachment));
  }

  TextAttachment& GetMutableTextAttachment(int index) {
    return text_attachments_[index];
  }

  FileAttachment& GetMutableFileAttachment(int index) {
    return file_attachments_[index];
  }

  WifiCredentialsAttachment& GetMutableWifiCredentialsAttachment(int index) {
    return wifi_credentials_attachments_[index];
  }

  // Returns the total number of attachments of all types.
  int GetAttachmentCount() const {
    return text_attachments_.size() + file_attachments_.size() +
           wifi_credentials_attachments_.size();
  }

  // Returns the total size of all attachments.
  int64_t GetTotalAttachmentsSize() const;

  // Returns the total size of all attachments on disk.
  int64_t GetStorageSize() const;

  // Returns true if there are any attachments.
  bool HasAttachments() const {
    return !text_attachments_.empty() || !file_attachments_.empty() ||
           !wifi_credentials_attachments_.empty();
  }

  // Clear the contents of all attachments, but leaving the attachments in
  // place.
  void ClearAttachments();

  // Delete all attachments.
  void Clear();

 private:
  std::vector<TextAttachment> text_attachments_;
  std::vector<FileAttachment> file_attachments_;
  std::vector<WifiCredentialsAttachment> wifi_credentials_attachments_;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_ATTACHMENT_CONTAINER_H_
