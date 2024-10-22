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

#ifndef THIRD_PARTY_NEARBY_SHARING_TEXT_ATTACHMENT_H_
#define THIRD_PARTY_NEARBY_SHARING_TEXT_ATTACHMENT_H_

#include <cstdint>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "sharing/attachment.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/wire_format.pb.h"

namespace nearby {
namespace sharing {

// Represents a text attachment.
class TextAttachment : public Attachment {
 public:
  using Type = nearby::sharing::service::proto::TextMetadata::Type;

  TextAttachment(Type type, std::string text_body,
                 std::optional<std::string> text_title,
                 std::optional<std::string> mime_type, int32_t batch_id = 0,
                 SourceType source_type = SourceType::kUnknown);
  TextAttachment(int64_t id, Type type, std::string text_title, int64_t size,
                 int32_t batch_id = 0,
                 SourceType source_type = SourceType::kUnknown);
  TextAttachment(int64_t id, Type type, std::string text_body,
                 std::string text_title, int64_t size, std::string mime_type,
                 int32_t batch_id, SourceType source_type);
  TextAttachment(const TextAttachment&) = default;
  TextAttachment(TextAttachment&&) = default;
  TextAttachment& operator=(const TextAttachment&) = default;
  TextAttachment& operator=(TextAttachment&&) = default;
  ~TextAttachment() override = default;

  absl::string_view text_body() const { return text_body_; }
  absl::string_view text_title() const { return text_title_; }
  Type type() const { return type_; }

  // Attachment:
  absl::string_view GetDescription() const override;
  ShareType GetShareType() const override;

  void set_text_body(std::string text_body);

  std::string mime_type() const { return mime_type_; }

 private:
  Type type_ = service::proto::TextMetadata::UNKNOWN;
  std::string text_title_;
  std::string text_body_;
  std::string mime_type_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_TEXT_ATTACHMENT_H_
