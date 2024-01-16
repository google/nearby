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

#ifndef THIRD_PARTY_NEARBY_SHARING_ATTACHMENT_INFO_H_
#define THIRD_PARTY_NEARBY_SHARING_ATTACHMENT_INFO_H_

#include <stdint.h>

#include <filesystem>  // NOLINT(build/c++17)
#include <optional>
#include <string>

namespace nearby {
namespace sharing {

// Ties associated information to an Attachment.
struct AttachmentInfo {
  AttachmentInfo();
  ~AttachmentInfo();

  AttachmentInfo(AttachmentInfo&&);
  AttachmentInfo& operator=(AttachmentInfo&&);

  std::optional<int64_t> payload_id;
  std::string text_body;
  std::filesystem::path file_path;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_ATTACHMENT_INFO_H_
