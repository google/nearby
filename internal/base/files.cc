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

#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <optional>
#include <system_error>  // NOLINT(build/c++11)

namespace nearby::sharing {

bool FileExists(const std::filesystem::path& path) {
  std::error_code error_code;
  if (std::filesystem::exists(path, error_code) &&
      !std::filesystem::is_directory(path, error_code)) {
    // is_directory returns false on error.
    return (!error_code);
  }
  return false;
}

std::optional<uintmax_t> GetFileSize(const std::filesystem::path& path) {
  if (!FileExists(path)) {
    return std::nullopt;
  }
  std::error_code error_code;
  uintmax_t size = std::filesystem::file_size(path, error_code);
  if (size == static_cast<uintmax_t>(-1)) {
    return std::nullopt;
  }
  return size;
}

bool DirectoryExists(const std::filesystem::path& path) {
  std::error_code error_code;
  if (std::filesystem::exists(path, error_code) &&
      std::filesystem::is_directory(path, error_code)) {
    return true;
  }
  return false;
}

bool RemoveFile(const std::filesystem::path& path) {
  if (!FileExists(path)) {
    return false;
  }
  std::error_code error_code;
  return std::filesystem::remove(path, error_code);
}

std::optional<std::filesystem::path> GetTemporaryDirectory() {
  std::error_code error_code;
  std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path(error_code);
  if (temp_dir.empty()) {
    return std::nullopt;
  }
  return temp_dir;
}

std::filesystem::path CurrentDirectory() {
  // temp_directory_path() returns empty path on error.
  std::error_code error_code;
  return std::filesystem::current_path(error_code);
}

bool Rename(const std::filesystem::path& old_path,
            const std::filesystem::path& new_path) {
  std::error_code error_code;
  std::filesystem::rename(old_path, new_path, error_code);
  if (error_code) {
    return false;
  }
  return true;
}

bool CreateDirectories(const std::filesystem::path& path) {
  std::error_code error_code;
  std::filesystem::create_directories(path, error_code);
  if (error_code) {
    return false;
  }
  return true;
}

bool CreateHardLink(const std::filesystem::path& target,
                    const std::filesystem::path& link_path) {
  std::error_code error_code;
  std::filesystem::create_hard_link(target, link_path, error_code);
  if (error_code) {
    return false;
  }
  return true;
}

}  // namespace nearby::sharing
