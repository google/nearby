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

#include <cstddef>
#include <cstdint>
#include <optional>

#include "internal/base/file_path.h"

namespace nearby {

// Utility functions for file operations..
class Files {
 public:
  // Returns true if path exists and is not a directory.
  static bool FileExists(const FilePath& path);

  // Returns the size of the file at path, or nullopt if not found or not a
  // file.
  static std::optional<uintmax_t> GetFileSize(const FilePath& path);

  // Returns true if path exists and is a directory.
  static bool DirectoryExists(const FilePath& path);

  // Removes the file at path and returns true.
  // Returns false if path does not exist, is not a file or cannot be removed.
  static bool RemoveFile(const FilePath& path);

  // Recursively removes the directory at path and returns true.
  // Returns false if path does not exist, is not a directory or cannot be
  // removed.
  static bool RemoveDirectory(const FilePath& path);

  // Returns path to a temporary directory if available.
  // If system defined temporary directory is not available, returns the current
  // directory.
  static FilePath GetTemporaryDirectory();

  // Returns path to the current directory.  On failure returns an empty path.
  static FilePath CurrentDirectory();

  // Renames the file at old_path to new_path.
  // Returns true on success.
  static bool Rename(const FilePath& old_path, const FilePath& new_path);

  // Creates all directory leading to path.
  // Returns true on success.
  static bool CreateDirectories(const FilePath& path);

  // Creates a hard link to target at link_path.
  // Returns true on success.
  static bool CreateHardLink(const FilePath& target, const FilePath& link_path);

  // Copies the file at old_path to new_path.
  // Returns true on success.
  static bool CopyFileSafely(const FilePath& old_path,
                             const FilePath& new_path);

  // Returns the available disk space in bytes for the given path.
  // Returns nullopt if the path does not exist or if the space cannot be
  // determined.
  static std::optional<size_t> GetAvailableDiskSpaceInBytes(
      const FilePath& path);
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_BASE_FILES_H_
