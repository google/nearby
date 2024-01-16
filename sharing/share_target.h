// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_SHARE_TARGET_H_
#define THIRD_PARTY_NEARBY_SHARING_SHARE_TARGET_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "internal/network/url.h"
#include "sharing/attachment.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/file_attachment.h"
#include "sharing/text_attachment.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby {
namespace sharing {

// A remote device.
struct ShareTarget {
 public:
  ShareTarget();
  ShareTarget(
      std::string device_name, ::nearby::network::Url image_url,
      ShareTargetType type, std::vector<TextAttachment> text_attachments,
      std::vector<FileAttachment> file_attachments,
      std::vector<WifiCredentialsAttachment> wifi_credentials_attachments,
      bool is_incoming, std::optional<std::string> full_name, bool is_known,
      std::optional<std::string> device_id, bool for_self_share);
  ShareTarget(const ShareTarget&);
  ShareTarget(ShareTarget&&);
  ShareTarget& operator=(const ShareTarget&);
  ShareTarget& operator=(ShareTarget&&);
  ~ShareTarget();

  bool has_attachments() const;
  std::vector<int64_t> GetAttachmentIds() const;
  std::vector<std::unique_ptr<Attachment>> GetAttachments() const;
  int64_t GetTotalAttachmentsSize() const;
  std::string ToString() const;

  int64_t id;
  std::string device_name;
  // Uri that points to an image of the ShareTarget, if one exists.
  std::optional<::nearby::network::Url> image_url;
  ShareTargetType type = ShareTargetType::kUnknown;
  std::vector<TextAttachment> text_attachments;
  std::vector<FileAttachment> file_attachments;
  std::vector<WifiCredentialsAttachment> wifi_credentials_attachments;
  bool is_incoming = false;
  std::optional<std::string> full_name;
  // True if the local device has the PublicCertificate this target is
  // advertising.
  bool is_known = false;
  std::optional<std::string> device_id;
  // True if the remote device is also owned by the current user.
  bool for_self_share = false;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_SHARE_TARGET_H_
