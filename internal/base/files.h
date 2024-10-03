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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_BASE_FILES_H_
#define THIRD_PARTY_NEARBY_INTERNAL_BASE_FILES_H_

#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <optional>

// This file contains exception safe wrappers to access common std::filesystem
// functions.
namespace nearby::sharing {

// Returns true if path exists and is not a directory.
bool FileExists(const std::filesystem::path& path);

// Returns the size of the file at path, or nullopt if not found or not a file.
std::optional<uintmax_t> GetFileSize(const std::filesystem::path& path);

// Returns true if path exists and is a directory.
bool DirectoryExists(const std::filesystem::path& path);

// Removes the file at path and returns true.
// Returns false if path does not exist, is not a file or cannot be removed.
bool RemoveFile(const std::filesystem::path& path);

// Returns path to a temporary directory if available.
std::optional<std::filesystem::path> GetTemporaryDirectory();

// Returns path to the current directory.  On failure returns an empty path.
std::filesystem::path CurrentDirectory();

// Renames the file at old_path to new_path.
// Returns true on success.
bool Rename(const std::filesystem::path& old_path,
            const std::filesystem::path& new_path);

// Creates all directory leading to path.
// Returns true on success.
bool CreateDirectories(const std::filesystem::path& path);

// Creates a hard link to target at link_path.
// Returns true on success.
bool CreateHardLink(const std::filesystem::path& target,
                    const std::filesystem::path& link_path);

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_INTERNAL_BASE_FILES_H_
