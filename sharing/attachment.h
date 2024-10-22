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

#ifndef THIRD_PARTY_NEARBY_SHARING_ATTACHMENT_H_
#define THIRD_PARTY_NEARBY_SHARING_ATTACHMENT_H_

#include <cstdint>

#include "absl/strings/string_view.h"
#include "sharing/common/nearby_share_enums.h"

namespace nearby {
namespace sharing {

// A single attachment to be sent by / received from a ShareTarget, can be
// either a file or text.
class Attachment {
 public:
  enum class Family {
    kFile,
    kText,
    kWifiCredentials,
    kMaxValue = kWifiCredentials
  };

  // TODO(b/269353084): Auto-generate codes when a new source type in
  // sharing_enums.proto is added.
  enum class SourceType {
    kUnknown,
    kContextMenu,
    kDragAndDrop,
    kSelectFilesButton,
    kPaste,
    kSelectFoldersButton,
    kMaxValue = kSelectFoldersButton
  };

  Attachment(Family family, int64_t size, int32_t batch_id,
             SourceType source_type);
  Attachment(int64_t id, Family family, int64_t size, int32_t batch_id,
             SourceType source_type);
  Attachment(const Attachment&);
  Attachment(Attachment&&);
  Attachment& operator=(const Attachment&);
  Attachment& operator=(Attachment&&);
  virtual ~Attachment();

  int64_t id() const { return id_; }
  Family family() const { return family_; }
  int64_t size() const { return size_; }
  void set_size(int64_t size) { size_ = size; }
  int32_t batch_id() const { return batch_id_; }
  SourceType source_type() const { return source_type_; }

  virtual absl::string_view GetDescription() const = 0;
  virtual ShareType GetShareType() const = 0;

 private:
  int64_t id_;
  Family family_;
  int64_t size_;
  int32_t batch_id_;
  SourceType source_type_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_ATTACHMENT_H_
