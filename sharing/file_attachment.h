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

#ifndef THIRD_PARTY_NEARBY_SHARING_FILE_ATTACHMENT_H_
#define THIRD_PARTY_NEARBY_SHARING_FILE_ATTACHMENT_H_

#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "sharing/attachment.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/wire_format.pb.h"

namespace nearby {
namespace sharing {

// A single attachment to be sent by / received from a |ShareTarget|, can be
// either a file or text.
class FileAttachment : public Attachment {
 public:
  using Type = nearby::sharing::service::proto::FileMetadata::Type;

  explicit FileAttachment(std::filesystem::path file_path,
                          absl::string_view mime_type = "",
                          std::string parent_folder = "", int32_t batch_id = 0,
                          SourceType source_type = SourceType::kUnknown);
  FileAttachment(int64_t id, int64_t size, std::string file_name,
                 std::string mime_type, Type type,
                 std::string parent_folder = "", int32_t batch_id = 0,
                 SourceType source_type = SourceType::kUnknown);
  FileAttachment(const FileAttachment&) = default;
  FileAttachment(FileAttachment&&) = default;
  FileAttachment& operator=(const FileAttachment&) = default;
  FileAttachment& operator=(FileAttachment&&) = default;
  ~FileAttachment() override = default;

  absl::string_view file_name() const { return file_name_; }
  absl::string_view mime_type() const { return mime_type_; }
  absl::string_view parent_folder() const { return parent_folder_; }
  Type type() const { return type_; }
  const std::optional<std::filesystem::path>& file_path() const {
    return file_path_;
  }

  // Attachment:
  absl::string_view GetDescription() const override;
  ShareType GetShareType() const override;

  void set_file_path(std::optional<std::filesystem::path> path) {
    file_path_ = std::move(path);
  }

 private:
  // File name should be in UTF8 format.
  std::string file_name_;
  std::string mime_type_;
  Type type_;
  std::optional<std::filesystem::path> file_path_;
  std::string parent_folder_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_FILE_ATTACHMENT_H_
