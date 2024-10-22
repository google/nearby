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

namespace nearby {
namespace sharing {
namespace {

int64_t CreateRandomId() {
  absl::BitGen gen;
  return absl::Uniform<int64_t>(gen, 1, std::numeric_limits<int64_t>::max());
}

}  // namespace

// TODO(b/258690183): Add unit tests for Attachment with same and different ids
Attachment::Attachment(Attachment::Family family, int64_t size,
                       int32_t batch_id, SourceType source_type)
    : id_(CreateRandomId()),
      family_(family),
      size_(size),
      batch_id_(batch_id),
      source_type_(source_type) {}

Attachment::Attachment(int64_t id, Attachment::Family family, int64_t size,
                       int32_t batch_id, SourceType source_type)
    : id_(id),
      family_(family),
      size_(size),
      batch_id_(batch_id),
      source_type_(source_type) {}

Attachment::Attachment(const Attachment&) = default;

Attachment::Attachment(Attachment&&) = default;

Attachment& Attachment::operator=(const Attachment&) = default;

Attachment& Attachment::operator=(Attachment&&) = default;

Attachment::~Attachment() = default;

}  // namespace sharing
}  // namespace nearby
