// Copyright 2022 Google LLC
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

#include <knownfolders.h>
#include <shlobj.h>
#include <windows.h>

#include <xstring>

#include "gtest/gtest.h"

// Can't run on google 3, I presume the SHGetKnownFolderPath
// fails.
#if 0
class ImplementationPlatformTests : public testing::Test
{
 protected:
  // You can define per-test set-up logic as usual.
  void SetUp() override
  {
    PWSTR basePath;

    SHGetKnownFolderPath(
        FOLDERID_Downloads,  //  rfid: A reference to the KNOWNFOLDERID that
                             //        identifies the folder.
        0,                   // dwFlags: Flags that specify special retrieval
                             //          options.
        NULL,                // hToken: An access token that represents a
                             //         particular user.
        &basePath);          // ppszPath: When this method returns, contains
                             //           the address of a pointer to a
                             //           null-terminated Unicode string that
                             //           specifies the path of the known
                             //           folder. The calling process is
                             //           responsible for freeing this resource
                             //           once it is no longer needed by
                             //           calling CoTaskMemFree, whether
                             //           SHGetKnownFolderPath succeeds or not.

    size_t bufferSize;
    wcstombs_s(&bufferSize, NULL, 0, basePath, 0);
    std::string fullpathUTF8(bufferSize, '\0');
    wcstombs_s(&bufferSize, fullpathUTF8.data(), bufferSize, basePath,
               _TRUNCATE);
    default_download_path_ = fullpathUTF8;
  }

  std::string default_download_path_;
};

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithEmptyStringArgumentsShouldReturnBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("");
  std::string file_name("");

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, default_download_path_);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithSlashParentFolderArgumentsShouldReturn\
BaseDownloadPath)
{
  // Arrange
  std::string parent_folder("/");
  std::string file_name("");

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, default_download_path_);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithBackslashParentFolderArgumentsShouldReturn\
BaseDownloadPath)
{
  // Arrange
  std::string parent_folder("\\");
  std::string file_name("");

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, default_download_path_);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithSlashFileNameArgumentsShouldReturnBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("");
  std::string file_name("/");

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, default_download_path_);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithBackslashFileNameArgumentsShouldReturn\
BaseDownloadPath)
{
  // Arrange
  std::string parent_folder("");
  std::string file_name("\\");

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  auto result_size = result.size();
  auto default_size = default_download_path_.size();

  // Assert
  EXPECT_EQ(result, default_download_path_);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithParentFolderShouldReturnParentFolder\
AppendedToBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("test_parent_folder");
  std::string file_name("");

  std::stringstream path("");
  path << default_download_path_.c_str() << "\\"
       << "test_parent_folder";

  std::string expected = path.str();

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithParentFolderStartingWithSlashArgumentsShouldReturn\
ParentFolderAppendedToBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("/test_parent_folder");
  std::string file_name("");

  std::stringstream path("");
  path << default_download_path_.c_str() << "\\"
       << "test_parent_folder";

  std::string expected = path.str();

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithParentFolderStartingWithBackslashArguments\
ShouldReturnParentFolderAppendedToBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("\\test_parent_folder");
  std::string file_name("");

  std::stringstream path("");
  path << default_download_path_.c_str() << "\\"
       << "test_parent_folder";

  std::string expected = path.str();

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithParentFolderEndingWithSlashArgumentsShouldReturn\
ParentFolderAppendedToBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("test_parent_folder/");
  std::string file_name("");

  std::stringstream path("");
  path << default_download_path_.c_str() << "\\"
       << "test_parent_folder";

  std::string expected = path.str();

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithParentFolderEndingWithBackslashArguments\
ShouldReturnParentFolderAppendedToBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("test_parent_folder\\");
  std::string file_name("");

  std::stringstream path("");
  path << default_download_path_.c_str() << "\\"
       << "test_parent_folder";

  std::string expected = path.str();

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithFileNameBeginningWithSlashArgumentsShouldReturn\
FileNameAppendedToBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("");
  std::string file_name("/test_file_name.name");

  std::stringstream path("");
  path << default_download_path_.c_str() << "\\"
       << "test_file_name.name";

  std::string expected = path.str();

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithFileNameBeginningWithBackslashArgumentsShouldReturn\
FileNameAppendedToBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("");
  std::string file_name("\\test_file_name.name");

  std::stringstream path("");
  path << default_download_path_.c_str() << "\\"
       << "test_file_name.name";

  std::string expected = path.str();

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithFileNameEndingWithSlashArgumentsShouldReturnFileName\
AppendedToBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("");
  std::string file_name("test_file_name.name/");

  std::stringstream path("");
  path << default_download_path_.c_str() << "\\"
       << "test_file_name.name";

  std::string expected = path.str();

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithFileNameEndingWithBackslashArgumentsShouldReturn\
FileNameAppendedToBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("");
  std::string file_name("test_file_name.name\\");

  std::stringstream path("");
  path << default_download_path_.c_str() << "\\"
       << "test_file_name.name";

  std::string expected = path.str();

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithParentFolderAndFileNameArgumentsShouldReturn\
ParentFolderAndFileNameAppendedToBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("test_parent_folder");
  std::string file_name("test_file_name.name");

  std::stringstream path("");
  path << default_download_path_.c_str() << "\\"
       << "test_parent_folder"
       << "\\"
       << "test_file_name.name";

  std::string expected = path.str();

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests,
       GetDownloadPathWithParentFolderEndingWithBackslashAndFileNameArguments\
ShouldReturnParentFolderAndFileNameAppendedToBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("test_parent_folder\\");
  std::string file_name("test_file_name.name");

  std::stringstream path("");
  path << default_download_path_.c_str() << "\\"
       << "test_parent_folder"
       << "\\"
       << "test_file_name.name";

  std::string expected = path.str();

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, expected);
}

TEST_F(
    ImplementationPlatformTests,
    GetDownloadPathWithFileNameStartingWithBackslashAndParentFolderArguments\
ShouldReturnParentFolderAndFileNameAppendedToBaseDownloadPath)
{
  // Arrange
  std::string parent_folder("test_parent_folder");
  std::string file_name("\\test_file_name.name");

  std::stringstream path("");
  path << default_download_path_.c_str() << "\\"
       << "test_parent_folder"
       << "\\"
       << "test_file_name.name";

  std::string expected = path.str();

  // Act
  auto result = location::nearby::api::ImplementationPlatform::GetDownloadPath(
      parent_folder, file_name);

  // Assert
  EXPECT_EQ(result, expected);
}
#endif
