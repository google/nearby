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

#include "internal/platform/implementation/platform.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/output_file.h"
#include "internal/platform/implementation/windows/device_paths.h"
#include "internal/platform/implementation/windows/file_path.h"

namespace nearby::api {
namespace {

using ::testing::EndsWith;

TEST(PlatformTest, CreateOutputFileWithUnixPathSeparator) {
  std::unique_ptr<OutputFile> output_file =
      ImplementationPlatform::CreateOutputFile("C:\\tmp\\path1/path2\\x.txt");
  EXPECT_NE(output_file, nullptr);
  EXPECT_TRUE(output_file->Write("test").Ok());
}

TEST(PlatformTest, GetCustomSavePathWithCustomPath) {
  std::string actual = ImplementationPlatform::GetCustomSavePath(
      "C:\\custom\\path", "sub_dir", "file.txt");
  EXPECT_EQ(actual, "C:/custom/path/sub_dir/file.txt");
}

TEST(PlatformTest, GetCustomSavePathWithEmptySavePathUsesDownloadsPath) {
  FilePath downloads_path = nearby::platform::windows::GetDownloadsPath();
  downloads_path.append(FilePath("sub_dir"));
  downloads_path.append(FilePath("file.txt"));
  std::string expected =
      windows::FilePath::GetCustomSavePath(downloads_path).ToString();

  std::string actual =
      ImplementationPlatform::GetCustomSavePath("", "sub_dir", "file.txt");
  EXPECT_EQ(actual, expected);
  EXPECT_THAT(actual, EndsWith("sub_dir/file.txt"));
}

TEST(PlatformTest, GetCustomSavePathWithEmptyParentFolder) {
  std::string actual = ImplementationPlatform::GetCustomSavePath(
      "C:\\custom\\path", "", "file.txt");
  EXPECT_EQ(actual, "C:/custom/path/file.txt");
}

TEST(PlatformTest, GetCustomSavePathWithEmptyFileName) {
  std::string actual = ImplementationPlatform::GetCustomSavePath(
      "C:\\custom\\path", "sub_dir", "");
  EXPECT_EQ(actual, "C:/custom/path/sub_dir");
}

TEST(PlatformTest, GetCustomSavePathWithEmptyParentFolderAndFileName) {
  std::string actual =
      ImplementationPlatform::GetCustomSavePath("C:\\custom\\path", "", "");
  EXPECT_EQ(actual, "C:/custom/path");
}

TEST(PlatformTest, GetCustomSavePathWithAllEmptyUsesDownloadsPath) {
  FilePath downloads_path = nearby::platform::windows::GetDownloadsPath();
  std::string expected =
      windows::FilePath::GetCustomSavePath(downloads_path).ToString();

  std::string actual = ImplementationPlatform::GetCustomSavePath("", "", "");
  EXPECT_EQ(actual, expected);
}

TEST(PlatformTest, GetCustomSavePathSanitizesIllegalCharacters) {
  std::string actual = ImplementationPlatform::GetCustomSavePath(
      "C:\\custom\\path", "sub_dir", "fi*le?.txt");
  EXPECT_EQ(actual, "C:/custom/path/sub_dir/fi_le_.txt");
}

}  // namespace
}  // namespace nearby::api
