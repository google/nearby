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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_BASE_MIME_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_BASE_MIME_H_

#include <string>

#include "absl/strings/string_view.h"

namespace nearby {
namespace utils {

std::string GetWellKnownMimeTypeFromExtension(absl::string_view extension);

}  // namespace utils
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_BASE_MIME_H_
