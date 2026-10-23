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

#include "internal/base/file_path_sanitize.h"

#include <cstddef>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"

namespace nearby {

std::string SanitizeRelativeFilePath(absl::string_view relative_path) {
  absl::string_view path_view = relative_path;

  // Remove everything after a '\0' character first (C-string style)
  size_t null_pos = path_view.find('\0');
  if (null_pos != absl::string_view::npos) {
    path_view = path_view.substr(0, null_pos);
  }

  // Remove leading '/' and '\'
  while (absl::ConsumePrefix(&path_view, "/") ||
         absl::ConsumePrefix(&path_view, "\\")) {
  }

  // Replace "\" with "/"
  std::string path = absl::StrReplaceAll(path_view, {{"\\", "/"}});

  // Collapse duplicate slashes "//" -> "/"
  while (absl::StrContains(path, "//")) {
    path = absl::StrReplaceAll(path, {{"//", "/"}});
  }

  // Remove instances of "../"
  while (absl::StrContains(path, "../")) {
    path = absl::StrReplaceAll(path, {{"../", ""}});
  }

  // Remove trailing ".."
  if (absl::EndsWith(path, "..")) {
    path.resize(path.size() - 2);
  }

  return path;
}

std::string SanitizeFileName(absl::string_view file_name) {
  // Remove everything after a '\0' character first (C-string style)
  size_t null_pos = file_name.find('\0');
  if (null_pos != absl::string_view::npos) {
    file_name = file_name.substr(0, null_pos);
  }

  // Replace "\" with "/"
  std::string replaced = absl::StrReplaceAll(file_name, {{"\\", "/"}});
  absl::string_view view = replaced;

  // Remove everything before the last "/" character
  size_t last_slash = view.find_last_of('/');
  if (last_slash != absl::string_view::npos) {
    view.remove_prefix(last_slash + 1);
  }

  return std::string(view);
}

}  // namespace nearby

