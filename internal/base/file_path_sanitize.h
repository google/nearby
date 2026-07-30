// Copyright 2026 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_BASE_FILE_PATH_SANITIZE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_BASE_FILE_PATH_SANITIZE_H_

#include <string>
#include "absl/strings/string_view.h"

namespace nearby {

// Normalize and sanitize a relative file path in a platform independent way.
// Remove everything after a '\0' character if one exists.
// All "\" are replaced with "/".
// Duplicate slashes "//" are collapsed to "/".
// Remove instances of "../".
// Remove any trailing ".." characters.
std::string SanitizeRelativeFilePath(absl::string_view relative_path);

// Sanitize a file name
// Remove everything after a '\0' character if one exists.
// All "\" are replaced with "/".
// Remove everything before the last "/" character if one exists.
std::string SanitizeFileName(absl::string_view file_name);

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_BASE_FILE_PATH_SANITIZE_H_
