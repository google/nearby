// Copyright 2022 Google LLC
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

#include "sharing/share_target.h"

#include <cinttypes>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "internal/network/url.h"
#include "sharing/attachment.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/file_attachment.h"
#include "sharing/text_attachment.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby {
namespace sharing {
namespace {
using ::nearby::network::Url;

// Used to generate device ID.
static int64_t kLastGeneratedId = 0;
}  // namespace

ShareTarget::ShareTarget() { id = ++kLastGeneratedId; }

ShareTarget::ShareTarget(
    std::string device_name, Url image_url, ShareTargetType type,
    std::vector<TextAttachment> text_attachments,
    std::vector<FileAttachment> file_attachments,
    std::vector<WifiCredentialsAttachment> wifi_credentials_attachments,
    bool is_incoming, std::optional<std::string> full_name, bool is_known,
    std::optional<std::string> device_id, bool for_self_share)
    : device_name(std::move(device_name)),
      image_url(std::move(image_url)),
      type(type),
      text_attachments(std::move(text_attachments)),
      file_attachments(std::move(file_attachments)),
      wifi_credentials_attachments(std::move(wifi_credentials_attachments)),
      is_incoming(is_incoming),
      full_name(std::move(full_name)),
      is_known(is_known),
      device_id(std::move(device_id)),
      for_self_share(for_self_share) {
  id = ++kLastGeneratedId;
}

ShareTarget::ShareTarget(const ShareTarget&) = default;

ShareTarget::ShareTarget(ShareTarget&&) = default;

ShareTarget& ShareTarget::operator=(const ShareTarget&) = default;

ShareTarget& ShareTarget::operator=(ShareTarget&&) = default;

ShareTarget::~ShareTarget() = default;

std::vector<int64_t> ShareTarget::GetAttachmentIds() const {
  std::vector<int64_t> attachment_ids;

  attachment_ids.reserve(file_attachments.size() + text_attachments.size() +
                         wifi_credentials_attachments.size());
  for (const auto& file : file_attachments) attachment_ids.push_back(file.id());

  for (const auto& text : text_attachments) attachment_ids.push_back(text.id());

  for (const auto& wifi_credentials : wifi_credentials_attachments)
    attachment_ids.push_back(wifi_credentials.id());

  return attachment_ids;
}

std::vector<std::unique_ptr<Attachment>> ShareTarget::GetAttachments() const {
  std::vector<std::unique_ptr<Attachment>> attachments;
  attachments.reserve(file_attachments.size() + text_attachments.size() +
                      wifi_credentials_attachments.size());
  for (const auto& file : file_attachments) {
    attachments.push_back(std::make_unique<FileAttachment>(file));
  }

  for (const auto& text : text_attachments) {
    attachments.push_back(std::make_unique<TextAttachment>(text));
  }

  for (const auto& wifi_credentials : wifi_credentials_attachments) {
    attachments.push_back(
        std::make_unique<WifiCredentialsAttachment>(wifi_credentials));
  }

  return attachments;
}

int64_t ShareTarget::GetTotalAttachmentsSize() const {
  int64_t size_in_bytes = 0;

  for (const auto& file : file_attachments) {
    size_in_bytes += file.size();
  }

  for (const auto& text : text_attachments) {
    size_in_bytes += text.size();
  }

  for (const auto& wifi_credentials : wifi_credentials_attachments) {
    size_in_bytes += wifi_credentials.size();
  }

  return size_in_bytes;
}

std::string ShareTarget::ToString() const {
  std::vector<std::string> fmt;

  fmt.push_back(absl::StrFormat("id: %" PRId64, id));
  fmt.push_back(absl::StrFormat("device_name: %s", device_name));
  if (full_name) {
    fmt.push_back(absl::StrFormat("full_name: %s", *full_name));
  }
  if (image_url) {
    fmt.push_back(absl::StrFormat("image_url: %s", image_url->GetUrlPath()));
  }
  if (device_id) {
    fmt.push_back(absl::StrFormat("device_id: %s", *device_id));
  }
  fmt.push_back(
      absl::StrFormat("file_attachments_size: %d", file_attachments.size()));
  fmt.push_back(
      absl::StrFormat("text_attachments_size: %d", text_attachments.size()));
  fmt.push_back(absl::StrFormat("wifi_credentials_attachments_size: %d",
                                wifi_credentials_attachments.size()));
  fmt.push_back(absl::StrFormat("is_known: %d", is_known));
  fmt.push_back(absl::StrFormat("is_incoming: %d", is_incoming));
  fmt.push_back(absl::StrFormat("for_self_share: %d", for_self_share));

  return absl::StrCat("ShareTarget<", absl::StrJoin(fmt, ", "), ">");
}

bool ShareTarget::has_attachments() const {
  return !text_attachments.empty() || !file_attachments.empty() ||
         !wifi_credentials_attachments.empty();
}

}  // namespace sharing
}  // namespace nearby
