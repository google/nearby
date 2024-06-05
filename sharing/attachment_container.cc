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
#include <string>
#include <utility>
#include <vector>
#include "sharing/file_attachment.h"
#include "sharing/text_attachment.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby::sharing {

AttachmentContainer::AttachmentContainer(
    std::vector<TextAttachment> text_attachments,
    std::vector<FileAttachment> file_attachments,
    std::vector<WifiCredentialsAttachment> wifi_credentials_attachments)
    : text_attachments_(std::move(text_attachments)),
      file_attachments_(std::move(file_attachments)),
      wifi_credentials_attachments_(std::move(wifi_credentials_attachments)) {}

int64_t AttachmentContainer::GetTotalAttachmentsSize() const {
  int64_t size_in_bytes = 0;

  for (const auto& file : file_attachments_) {
    size_in_bytes += file.size();
  }

  for (const auto& text : text_attachments_) {
    size_in_bytes += text.size();
  }

  for (const auto& wifi_credentials : wifi_credentials_attachments_) {
    size_in_bytes += wifi_credentials.size();
  }

  return size_in_bytes;
}

void AttachmentContainer::ClearAttachments() {
  // Reset file paths for file attachments.
  for (auto& file : file_attachments_)
    file.set_file_path(std::nullopt);

  // Reset body of text attachments.
  for (auto& text : text_attachments_)
    text.set_text_body(std::string());

  // Reset password of Wi-Fi credentials attachments.
  for (auto& wifi_credentials : wifi_credentials_attachments_) {
    wifi_credentials.set_password(std::string());
    wifi_credentials.set_is_hidden(false);
  }
}

std::vector<int64_t> AttachmentContainer::GetAttachmentIds() const {
  std::vector<int64_t> attachment_ids;

  attachment_ids.reserve(GetAttachmentCount());
  for (const auto& file : file_attachments_)
    attachment_ids.push_back(file.id());

  for (const auto& text : text_attachments_)
    attachment_ids.push_back(text.id());

  for (const auto& wifi_credentials : wifi_credentials_attachments_)
    attachment_ids.push_back(wifi_credentials.id());

  return attachment_ids;
}

}  // namespace nearby::sharing
