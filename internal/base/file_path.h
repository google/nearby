// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_BASE_FILE_PATH_H_
#define THIRD_PARTY_NEARBY_INTERNAL_BASE_FILE_PATH_H_

#include <filesystem>  // NOLINT
#include <string>
#include <utility>

#include "absl/strings/string_view.h"

namespace nearby {

// A replacement for std::filesystem::path that is exception safe, and is safe
// for use with Unicode paths in Windows.
class FilePath {
 public:
  FilePath() = default;
  FilePath(const FilePath&) = default;
  FilePath& operator=(const FilePath&) = default;
  FilePath(FilePath&&) = default;
  FilePath& operator=(FilePath&&) = default;

  // Creates a FilePath from a UTF-8 encoded string.
  // The `path` must be a valid UTF-8 sequence, otherwise the behavior is
  // undefined.
  explicit FilePath(absl::string_view path);
  // Creates a FilePath from a unicode string.
  explicit FilePath(std::wstring_view path);

  // Returns the path as a UTF-8 encoded string.
  std::string ToString() const;
  // Returns the path as a unicode string.
  std::wstring ToWideString() const;

  // Returns true if the path is empty.
  bool IsEmpty() const;

  // Appends the given `subpath` to this path using a path separator..
  FilePath& append(const FilePath& subpath);

  // Returns the last component of this path.
  FilePath GetFileName() const;

  // Returns the extension of the last component of this path.
  // The return extension includes the "." prefix.
  FilePath GetExtension() const;

  // Returns the path of the parent directory of this path.
  FilePath GetParentPath() const;

  // TODO: b/418255947  - Remove after migration is complete.
  std::filesystem::path GetPath() const { return path_; }

  friend auto operator<=>(const FilePath& lhs, const FilePath& rhs) = default;

  // Hash function for absl containers.
  template <typename H>
  friend H AbslHashValue(H h, const FilePath& path) {
    return H::combine(std::move(h), path.path_);
  }

 private:
  static FilePath FromPath(std::filesystem::path path) {
    FilePath result;
    result.path_ = std::move(path);
    return result;
  }

  std::filesystem::path path_;

  friend class Files;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_BASE_FILE_PATH_H_
