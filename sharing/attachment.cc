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

#include "sharing/attachment.h"

#include <cstdint>
#include <limits>

#include "absl/random/random.h"
#include "proto/sharing_enums.pb.h"

namespace nearby::sharing {
namespace {

using ::location::nearby::proto::sharing::AttachmentSourceType;

int64_t CreateRandomId() {
  absl::BitGen gen;
  return absl::Uniform<int64_t>(gen, 1, std::numeric_limits<int64_t>::max());
}

}  // namespace

Attachment::Attachment(Attachment::Family family, int64_t size,
                       int32_t batch_id, AttachmentSourceType source_type)
    : Attachment(/*id=*/0, family, size, batch_id, source_type) {}

Attachment::Attachment(int64_t id, Attachment::Family family, int64_t size,
                       int32_t batch_id, AttachmentSourceType source_type)
    : id_(id == 0 ? CreateRandomId() : id),
      family_(family),
      batch_id_(batch_id),
      source_type_(source_type),
      size_(size) {}

}  // namespace nearby::sharing
