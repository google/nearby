// Copyright 2020 Google LLC
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

#ifndef CORE_INTERNAL_OFFLINE_FRAMES_VALIDATOR_H_
#define CORE_INTERNAL_OFFLINE_FRAMES_VALIDATOR_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace connections {
namespace parser {

constexpr absl::string_view kIllegalFileNamePatterns[] = {":", "/", "\\"};

constexpr absl::string_view kIllegalParentFolderPatterns[] = {":", ".."};

const size_t kIllegalFileNamePatternsSize =
    sizeof(kIllegalFileNamePatterns) / sizeof(*kIllegalFileNamePatterns);
const size_t kIllegalParentFolderPatternsSize =
    sizeof(kIllegalParentFolderPatterns) /
    sizeof(*kIllegalParentFolderPatterns);

Exception EnsureValidOfflineFrame(
    const location::nearby::connections::OfflineFrame& offline_frame);

}  // namespace parser
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_OFFLINE_FRAMES_VALIDATOR_H_
