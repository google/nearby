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

#include "internal/base/file_path.h"

#include <filesystem>  // NOLINT
#include <string>

#include "absl/strings/string_view.h"
#include "internal/base/compatible_u8_string.h"

namespace nearby {

FilePath::FilePath(absl::string_view path)
    : path_(std::filesystem::u8path(path.begin(), path.end())) {}

FilePath::FilePath(std::wstring_view path)
    : path_(path) {}

std::string FilePath::ToString() const {
  return GetCompatibleU8String(path_.u8string());
}

std::wstring FilePath::ToWideString() const {
  return path_.wstring();
}

FilePath& FilePath::append(const FilePath& subpath) {
  path_ /= subpath.path_;
  return *this;
}

FilePath FilePath::GetParentPath() const {
  return FilePath(path_.parent_path().wstring());
}

}  // namespace nearby
