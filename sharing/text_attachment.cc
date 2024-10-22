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

#include "sharing/text_attachment.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/network/url.h"
#include "sharing/attachment.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/wire_format.pb.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::network::Url;

// Tries to get a valid host name from the |text|. Returns nullopt otherwise.
std::optional<std::string> GetHostFromText(absl::string_view text) {
  absl::StatusOr<Url> url = Url::Create(text);
  if (!url.ok() || url->GetHostName().empty()) return std::nullopt;

  return absl::StrCat(url->GetHostName());
}

// Masks the given |number| depending on the string length:
//  - length <= 4: Masks all characters
//  - 4 < length <= 6 Masks the last 4 characters
//  - 6 < length <= 10: Skips the first 2 and masks the following 4 characters
//  - length > 10: Skips the first 2 and last 4 characters. Masks the rest of
//    the string
// Note: We're assuming a formatted phone number and won't try to reformat to
// E164 like on Android as there's no easy way of determining the intended
// region for the phone number.
std::string MaskPhoneNumber(const std::string& number) {
  constexpr int kMinMaskedDigits = 4;
  constexpr int kMaxLeadingDigits = 2;
  constexpr int kMaxTailingDigits = 4;
  constexpr int kLengthWithNoLeadingDigits = kMinMaskedDigits;
  constexpr int kLengthWithNoTailingDigits =
      kMinMaskedDigits + kMaxLeadingDigits;

  if (number.empty()) return number;

  std::string result = number;
  bool has_plus = false;
  if (number[0] == '+') {
    result = number.substr(1);
    has_plus = true;
  }

  // First calculate how many digits we would mask having exactly
  // kMinMaskedDigits digits masked.
  int leading_digits = 0;
  if (result.length() > kLengthWithNoLeadingDigits)
    leading_digits = result.length() - kLengthWithNoLeadingDigits;

  int tailing_digits = 0;
  if (result.length() > kLengthWithNoTailingDigits)
    tailing_digits = result.length() - kLengthWithNoTailingDigits;

  // Now limit resulting numbers of digits to maximally allowed values.
  leading_digits = std::min(kMaxLeadingDigits, leading_digits);
  tailing_digits = std::min(kMaxTailingDigits, tailing_digits);
  int masked_digits = result.length() - leading_digits - tailing_digits;

  return absl::StrCat((has_plus ? "+" : ""), result.substr(0, leading_digits),
                      std::string(masked_digits, 'x'),
                      result.substr(result.length() - tailing_digits));
}

std::string GetTextTitle(const std::string& text_body,
                         TextAttachment::Type type) {
  constexpr size_t kMaxPreviewTextLength = 32;

  switch (type) {
    case service::proto::TextMetadata::URL: {
      std::optional<std::string> host = GetHostFromText(text_body);
      if (host.has_value()) return *host;

      break;
    }
    case service::proto::TextMetadata::PHONE_NUMBER:
      return MaskPhoneNumber(text_body);
    default:
      break;
  }

  if (text_body.size() > kMaxPreviewTextLength)
    return absl::StrCat(text_body.substr(0, kMaxPreviewTextLength), "…");

  return text_body;
}

}  // namespace

TextAttachment::TextAttachment(Type type, std::string text_body,
                               std::optional<std::string> text_title,
                               std::optional<std::string> mime_type,
                               int32_t batch_id, SourceType source_type)
    : Attachment(Attachment::Family::kText, text_body.size(), batch_id,
                 source_type),
      type_(type),
      text_title_(text_title.has_value() && !text_title->empty()
                      ? *text_title
                      : GetTextTitle(text_body, type)),
      text_body_(std::move(text_body)),
      mime_type_(mime_type ? *mime_type : std::string()) {}

TextAttachment::TextAttachment(int64_t id, Type type, std::string text_title,
                               int64_t size, int32_t batch_id,
                               SourceType source_type)
    : Attachment(id, Attachment::Family::kText, size, batch_id, source_type),
      type_(type),
      text_title_(std::move(text_title)) {}

TextAttachment::TextAttachment(int64_t id, Type type, std::string text_body,
                               std::string text_title, int64_t size,
                               std::string mime_type, int32_t batch_id,
                               SourceType source_type)
    : Attachment(id, Attachment::Family::kText, size, batch_id, source_type),
      type_(type),
      text_title_(std::move(text_title)),
      text_body_(std::move(text_body)),
      mime_type_(std::move(mime_type)) {}

absl::string_view TextAttachment::GetDescription() const { return text_title_; }

ShareType TextAttachment::GetShareType() const {
  switch (type()) {
    case service::proto::TextMetadata::URL:
      if (mime_type_ == "application/vnd.google-apps.document") {
        return ShareType::kGoogleDocsFile;
      } else if (mime_type_ == "application/vnd.google-apps.spreadsheet") {
        return ShareType::kGoogleSheetsFile;
      } else if (mime_type_ == "application/vnd.google-apps.presentation") {
        return ShareType::kGoogleSlidesFile;
      } else {
        return ShareType::kUrl;
      }
    case service::proto::TextMetadata::ADDRESS:
      return ShareType::kAddress;
    case service::proto::TextMetadata::PHONE_NUMBER:
      return ShareType::kPhone;
    default:
      return ShareType::kText;
  }
}

void TextAttachment::set_text_body(std::string text_body) {
  text_body_ = std::move(text_body);
  text_title_ = GetTextTitle(text_body_, type_);
}

}  // namespace sharing
}  // namespace nearby
