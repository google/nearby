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

#include "sharing/file_attachment.h"

#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "sharing/attachment.h"
#include "sharing/common/compatible_u8_string.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/internal/base/mime.h"
#include "sharing/proto/wire_format.pb.h"

namespace nearby {
namespace sharing {
namespace {

FileAttachment::Type FileAttachmentTypeFromMimeType(
    absl::string_view mime_type) {
  if (absl::StartsWith(mime_type, "image/"))
    return service::proto::FileMetadata::IMAGE;

  if (absl::StartsWith(mime_type, "video/"))
    return service::proto::FileMetadata::VIDEO;

  if (absl::StartsWith(mime_type, "audio/"))
    return service::proto::FileMetadata::AUDIO;

  return service::proto::FileMetadata::UNKNOWN;
}

std::string MimeTypeFromPath(const std::filesystem::path& path) {
  std::string extension = path.extension().string();
  return extension.empty() ? "application/octet-stream"
                           : nearby::utils::GetWellKnownMimeTypeFromExtension(
                                 extension.substr(1));
}

}  // namespace

FileAttachment::FileAttachment(std::filesystem::path file_path,
                               absl::string_view mime_type,
                               std::string parent_folder, int32_t batch_id,
                               SourceType source_type)
    : Attachment(Attachment::Family::kFile, /*size=*/0, batch_id, source_type),
      mime_type_(mime_type.empty() ? MimeTypeFromPath(file_path) : mime_type),
      type_(FileAttachmentTypeFromMimeType(mime_type_)),
      file_path_(std::move(file_path)),
      parent_folder_(std::move(parent_folder)) {
  file_name_ =
      GetCompatibleU8String(file_path_.value_or(L"").filename().u8string());
}

FileAttachment::FileAttachment(int64_t id, int64_t size, std::string file_name,
                               std::string mime_type, Type type,
                               std::string parent_folder, int32_t batch_id,
                               SourceType source_type)
    : Attachment(id, Attachment::Family::kFile, size, batch_id, source_type),
      file_name_(std::move(file_name)),
      mime_type_(std::move(mime_type)),
      type_(type),
      parent_folder_(std::move(parent_folder)) {}

absl::string_view FileAttachment::GetDescription() const { return file_name_; }

ShareType FileAttachment::GetShareType() const {
  switch (type()) {
    case service::proto::FileMetadata::IMAGE:
      return ShareType::kImageFile;
    case service::proto::FileMetadata::VIDEO:
      return ShareType::kVideoFile;
    case service::proto::FileMetadata::AUDIO:
      return ShareType::kAudioFile;
    default:
      break;
  }

  // Try matching on mime type if the attachment type is unrecognized.
  if (mime_type() == "application/pdf") {
    return ShareType::kPdfFile;
  } else if (mime_type() == "application/vnd.google-apps.document") {
    return ShareType::kGoogleDocsFile;
  } else if (mime_type() == "application/vnd.google-apps.spreadsheet") {
    return ShareType::kGoogleSheetsFile;
  } else if (mime_type() == "application/vnd.google-apps.presentation") {
    return ShareType::kGoogleSlidesFile;
  } else if (mime_type() == "text/plain") {
    return ShareType::kTextFile;
  } else {
    return ShareType::kUnknownFile;
  }
}

}  // namespace sharing
}  // namespace nearby
