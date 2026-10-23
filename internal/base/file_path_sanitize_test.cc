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

#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"

namespace nearby {
namespace {

TEST(FilePathSanitizeTest, SanitizeRelativeFilePath) {
  // Normal paths
  EXPECT_EQ(SanitizeRelativeFilePath("a/b/c"), "a/b/c");
  EXPECT_EQ(SanitizeRelativeFilePath("a"), "a");
  EXPECT_EQ(SanitizeRelativeFilePath(""), "");

  // Backslash replacement
  EXPECT_EQ(SanitizeRelativeFilePath("a\\b\\c"), "a/b/c");
  EXPECT_EQ(SanitizeRelativeFilePath("a\\b/c"), "a/b/c");

  // Leading slashes
  EXPECT_EQ(SanitizeRelativeFilePath("/a/b/c"), "a/b/c");
  EXPECT_EQ(SanitizeRelativeFilePath("///a/b"), "a/b");
  EXPECT_EQ(SanitizeRelativeFilePath("\\\\a\\b"), "a/b");
  EXPECT_EQ(SanitizeRelativeFilePath("///"), "");

  // Trailing slashes (should be preserved if they don't follow ..)
  EXPECT_EQ(SanitizeRelativeFilePath("a/b/"), "a/b/");

  // Trailing ".."
  EXPECT_EQ(SanitizeRelativeFilePath("a/b/.."), "a/b/");
  EXPECT_EQ(SanitizeRelativeFilePath(".."), "");
  EXPECT_EQ(SanitizeRelativeFilePath("a/.."), "a/");
  // Literal trailing ".." characters removal (as per comment "Remove any
  // trailing ".." characters")
  EXPECT_EQ(SanitizeRelativeFilePath("a.."), "a");
  EXPECT_EQ(SanitizeRelativeFilePath("a/b.."), "a/b");
  EXPECT_EQ(SanitizeRelativeFilePath("a/b..."), "a/b.");

  // Remove "../"
  EXPECT_EQ(SanitizeRelativeFilePath("a/../b"), "a/b");
  EXPECT_EQ(SanitizeRelativeFilePath("../a/b"), "a/b");
  EXPECT_EQ(SanitizeRelativeFilePath("a/b/../"), "a/b/");
  EXPECT_EQ(SanitizeRelativeFilePath("a/../../b"), "a/b");
  EXPECT_EQ(SanitizeRelativeFilePath("a/../b/../c"), "a/b/c");

  // Duplicate slashes and edge cases with ".."
  EXPECT_EQ(SanitizeRelativeFilePath("..//a"), "a");
  EXPECT_EQ(SanitizeRelativeFilePath("a//b"), "a/b");
  EXPECT_EQ(SanitizeRelativeFilePath("a///..//b"), "a/b");

  // Null byte truncation
  EXPECT_EQ(SanitizeRelativeFilePath(absl::string_view("a/..\0/b", 7)), "a/");
}

TEST(FilePathSanitizeTest, SanitizeFileName) {
  // Normal file names
  EXPECT_EQ(SanitizeFileName("file.txt"), "file.txt");
  EXPECT_EQ(SanitizeFileName("a"), "a");
  EXPECT_EQ(SanitizeFileName(""), "");

  // Path removal (slash and backslash)
  EXPECT_EQ(SanitizeFileName("dir/file.txt"), "file.txt");
  EXPECT_EQ(SanitizeFileName("dir\\file.txt"), "file.txt");
  EXPECT_EQ(SanitizeFileName("dir1/dir2\\file.txt"), "file.txt");
  EXPECT_EQ(SanitizeFileName("dir1\\dir2/file.txt"), "file.txt");
  EXPECT_EQ(SanitizeFileName("/file.txt"), "file.txt");
  EXPECT_EQ(SanitizeFileName("\\file.txt"), "file.txt");

  // Null byte truncation
  // Using string_view constructor that takes size to allow null bytes
  EXPECT_EQ(SanitizeFileName(absl::string_view("file.txt\0bad", 12)),
            "file.txt");
  EXPECT_EQ(SanitizeFileName(absl::string_view("file.txt\0", 9)), "file.txt");
  EXPECT_EQ(SanitizeFileName(absl::string_view("\0bad", 4)), "");

  // Combined path removal and null byte truncation
  EXPECT_EQ(SanitizeFileName(absl::string_view("dir/file.txt\0bad", 16)),
            "file.txt");
  EXPECT_EQ(SanitizeFileName(absl::string_view("dir\\file.txt\0bad", 16)),
            "file.txt");

  // Null byte before slash
  EXPECT_EQ(SanitizeFileName(absl::string_view("dir\0file.txt", 12)), "dir");
  EXPECT_EQ(SanitizeFileName(absl::string_view("dir/subdir\0file.txt", 19)),
            "subdir");
  EXPECT_EQ(SanitizeFileName(absl::string_view("dir\0/file.txt", 13)), "dir");
  EXPECT_EQ(SanitizeFileName(absl::string_view("dir/subdir\0/file.txt", 20)),
            "subdir");
}

}  // namespace
}  // namespace nearby
