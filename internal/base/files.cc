// Copyright 2024 Google LLC
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

#include "internal/base/files.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <optional>
#include <system_error>  // NOLINT(build/c++11)

#include "internal/base/file_path.h"
#include "internal/platform/logging.h"

namespace nearby {

bool Files::FileExists(const FilePath& path) {
  std::error_code error_code;
  if (std::filesystem::exists(path.path_, error_code) &&
      !std::filesystem::is_directory(path.path_, error_code)) {
    // is_directory returns false on error.
    return (!error_code);
  }
  return false;
}

std::optional<uintmax_t> Files::GetFileSize(const FilePath& path) {
  if (!FileExists(path)) {
    return std::nullopt;
  }
  std::error_code error_code;
  uintmax_t size = std::filesystem::file_size(path.path_, error_code);
  if (size == static_cast<uintmax_t>(-1)) {
    return std::nullopt;
  }
  return size;
}

bool Files::DirectoryExists(const FilePath& path) {
  std::error_code error_code;
  if (std::filesystem::exists(path.path_, error_code) &&
      std::filesystem::is_directory(path.path_, error_code)) {
    return true;
  }
  return false;
}

bool Files::RemoveFile(const FilePath& path) {
  if (!FileExists(path)) {
    return false;
  }
  std::error_code error_code;
  return std::filesystem::remove(path.path_, error_code);
}

bool Files::RemoveDirectory(const FilePath& path) {
  // TODO: b/418255947 - Should return false if path is not a directory, and
  // true if path does not exist.
  if (!DirectoryExists(path)) {
    return false;
  }
  std::error_code error_code;
  return std::filesystem::remove_all(path.path_, error_code) != -1;
}

FilePath Files::GetTemporaryDirectory() {
  std::error_code error_code;
  std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path(error_code);
  if (temp_dir.empty()) {
    return CurrentDirectory();
  }
  return FilePath{temp_dir.wstring()};
}

FilePath Files::CurrentDirectory() {
  // temp_directory_path() returns empty path on error.
  std::error_code error_code;
  return FilePath{std::filesystem::current_path(error_code).wstring()};
}

bool Files::Rename(const FilePath& old_path, const FilePath& new_path) {
  std::error_code error_code;
  std::filesystem::rename(old_path.path_, new_path.path_, error_code);
  if (error_code) {
    VLOG(1) << "Failed to rename file: " << error_code.message();
    return false;
  }
  return true;
}

bool Files::CreateDirectories(const FilePath& path) {
  std::error_code error_code;
  std::filesystem::create_directories(path.path_, error_code);
  if (error_code) {
    VLOG(1) << "Failed to create directories: " << error_code.value();
    return false;
  }
  return true;
}

bool Files::CreateHardLink(const FilePath& target, const FilePath& link_path) {
  std::error_code error_code;
  std::filesystem::create_hard_link(target.path_, link_path.path_, error_code);
  if (error_code) {
    VLOG(1) << "Failed to create hard link: " << error_code.value();
    return false;
  }
  return true;
}

bool Files::CopyFileSafely(const FilePath& old_path, const FilePath& new_path) {
  std::error_code error_code;
  std::filesystem::copy(old_path.path_, new_path.path_, error_code);
  if (error_code) {
    VLOG(1) << "Failed to copy file: " << error_code.value();
    return false;
  }
  return true;
}

std::optional<size_t> Files::GetAvailableDiskSpaceInBytes(
    const FilePath& path) {
  std::error_code error_code;
  std::filesystem::space_info space_info =
      std::filesystem::space(path.path_, error_code);
  if (error_code.value() == 0) {
    return space_info.available;
  }
  VLOG(1) << "Failed to get available disk space: " << error_code.value();
  return std::nullopt;
}

}  // namespace nearby
