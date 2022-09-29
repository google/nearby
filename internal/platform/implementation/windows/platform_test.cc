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

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

#include <xstring>
#include <fstream>

#include "gtest/gtest.h"

namespace location::nearby::windows {

namespace {
constexpr absl::string_view kFileName("/increment_file_test.txt");
constexpr absl::string_view kFirstIterationFileName(
    "/increment_file_test (1).txt");
constexpr absl::string_view kSecondIterationFileName(
    "/increment_file_test (2).txt");
constexpr absl::string_view kThirdIterationFileName(
    "/increment_file_test (3).txt");
constexpr absl::string_view kNoDotsFileName("/incrementfiletesttxt");
constexpr absl::string_view kOneIterationNoDotsFileName(
    "/incrementfiletesttxt (1)");
constexpr absl::string_view kMultipleDotsFileName("/increment.file.test.txt");
constexpr absl::string_view kOneIterationMultipleDotsFileName(
    "/increment.file.test (1).txt");
constexpr absl::string_view kImmediateEscape("../");
constexpr absl::string_view kLongEscapeBackSlash("..\\test\\..\\..\\test");
constexpr absl::string_view kTwoLevelFolder("/test/test");
constexpr absl::string_view kLongEscapeSlash("../test/../../test");
constexpr absl::string_view kLongEscapeMixedSlash("../test\\..\\../test");
constexpr absl::string_view kLongEscapeEndingEscape("../test/../../test/..");
constexpr absl::string_view kLongEscapeEndingEscapeWithSlash(
    "../test/../../test/../../../");
}  // namespace

using ::location::nearby::api::ImplementationPlatform;

// Can't run on google 3, I presume the SHGetKnownFolderPath
// fails.
class ImplementationPlatformTests : public testing::Test {
 protected:
  // You can define per-test set-up logic as usual.
  ImplementationPlatformTests() {
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
    default_download_path_.resize(bufferSize - 1, '\0');
    wcstombs_s(&bufferSize, default_download_path_.data(), bufferSize, basePath,
               _TRUNCATE);

    std::replace(default_download_path_.begin(), default_download_path_.end(),
                 '\\', '/');
  }

  std::string default_download_path_;
};

TEST_F(ImplementationPlatformTests,
       DISABLED_GetDownloadPathWithEmptyStringArguments\
ShouldReturnBaseDownloadPath) {
  std::string parent_folder("");
  std::string file_name("");

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, default_download_path_);
}  // NOLINT false lint error here

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithSlashParent\
FolderArgumentsShouldReturnBaseDownloadPath) {
  std::string parent_folder("/");
  std::string file_name("");

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, default_download_path_);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithBackslashParent\
FolderArgumentsShouldReturnBaseDownloadPath) {
  std::string parent_folder("\\");
  std::string file_name("");

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, default_download_path_);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithAttemptToEscape\
UsersDownloadFolderShouldReturnDownloadPathNotEscapingUsersDownloadFolder) {
  std::string parent_folder(kImmediateEscape);
  std::string file_name("");

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, default_download_path_);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithMultiple\
AttemptsToEscapeUsersDownloadFolderWithBackslashShouldReturnDownloadPath\
NotEscapingUsersDownloadFolder) {
  std::string parent_folder(kLongEscapeBackSlash);
  std::string file_name("");

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, default_download_path_ + kTwoLevelFolder.data());
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithMultiple\
AttemptsToEscapeUsersDownloadFolderShouldReturnDownloadPathNotEscapingUsers\
DownloadFolder) {
  std::string parent_folder(kLongEscapeSlash);
  std::string file_name("");

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, default_download_path_ + kTwoLevelFolder.data());
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithMultiple\
AttemptsToEscapeUsersDownloadFolderWithMixedSlashShouldReturnDownloadPath\
NotEscapingUsersDownloadFolder) {
  std::string parent_folder(kLongEscapeMixedSlash);
  std::string file_name("");

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, default_download_path_ + kTwoLevelFolder.data());
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithMultiple\
AttemptsToEscapeUsersDownloadFolderWithEndingEscapeShouldReturnDownload\
PathNotEscapingUsersDownloadFolder) {
  std::string parent_folder(kLongEscapeEndingEscape);
  std::string file_name("");

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, default_download_path_ + kTwoLevelFolder.data());
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithMultiple\
AttemptsToEscapeUsersDownloadFolderWithEndingSlashShouldReturnDownloadPathNot\
EscapingUsersDownloadFolder) {
  std::string parent_folder(kLongEscapeEndingEscapeWithSlash);
  std::string file_name("");

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, default_download_path_ + kTwoLevelFolder.data());
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithSlashFileName\
ArgumentsShouldReturnBaseDownloadPath) {
  std::string parent_folder("");
  std::string file_name("/");

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, default_download_path_);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithBackslashFile\
NameArgumentsShouldReturnBaseDownloadPath) {
  std::string parent_folder("");
  std::string file_name("\\");

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  auto result_size = result.size();
  auto default_size = default_download_path_.size();

  EXPECT_EQ(result, default_download_path_);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithParentFolder\
ShouldReturnParentFolderAppendedToBaseDownloadPath) {
  std::string parent_folder("test_parent_folder");
  std::string file_name("");

  std::stringstream path("");
  path << default_download_path_ << "/"
       << "test_parent_folder";

  std::string expected = path.str();

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithParentFolder\
StartingWithSlashArgumentsShouldReturnParentFolderAppendedToBaseDownloadPath) {
  std::string parent_folder("/test_parent_folder");
  std::string file_name("");

  std::stringstream path("");
  path << default_download_path_ << "/"
       << "test_parent_folder";

  std::string expected = path.str();

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithParentFolder\
StartingWithBackslashArgumentsShouldReturnParentFolderAppendedToBase\
DownloadPath) {
  std::string parent_folder("\\test_parent_folder");
  std::string file_name("");

  std::stringstream path("");
  path << default_download_path_ << "/"
       << "test_parent_folder";

  std::string expected = path.str();

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithParentFolder\
EndingWithSlashArgumentsShouldReturnParentFolderAppendedToBaseDownloadPath) {
  std::string parent_folder("test_parent_folder/");
  std::string file_name("");

  std::stringstream path("");
  path << default_download_path_ << "/"
       << "test_parent_folder";

  std::string expected = path.str();

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithParentFolder\
EndingWithBackslashArguments\
ShouldReturnParentFolderAppendedToBaseDownloadPath) {
  std::string parent_folder("test_parent_folder\\");
  std::string file_name("");

  std::stringstream path("");
  path << default_download_path_ << "/"
       << "test_parent_folder";

  std::string expected = path.str();

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithFileName\
BeginningWithSlashArgumentsShouldReturnFileNameAppendedToBaseDownloadPath) {
  std::string parent_folder("");
  std::string file_name("/test_file_name.name");

  std::stringstream path("");
  path << default_download_path_ << "/"
       << "test_file_name.name";

  std::string expected = path.str();

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithFileName\
BeginningWithBackslashArgumentsShouldReturnFileNameAppendedToBaseDownloadPath) {
  std::string parent_folder("");
  std::string file_name("\\test_file_name.name");

  std::stringstream path("");
  path << default_download_path_ << "/"
       << "test_file_name.name";

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, path.str().c_str());
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithFileNameEnding\
WithSlashArgumentsShouldReturnFileNameAppendedToBaseDownloadPath) {
  std::string parent_folder("");
  std::string file_name("test_file_name.name/");

  std::stringstream path("");
  path << default_download_path_ << "/"
       << "test_file_name.name";

  std::string expected = path.str();

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithFileNameEnding\
WithBackslashArgumentsShouldReturnFileNameAppendedToBaseDownloadPath) {
  std::string parent_folder("");
  std::string file_name("test_file_name.name\\");

  std::stringstream path("");
  path << default_download_path_ << "/"
       << "test_file_name.name";

  std::string expected = path.str();

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithParentFolderAnd\
FileNameArgumentsShould\
ReturnParentFolderAndFileNameAppendedToBaseDownloadPath) {
  std::string parent_folder("test_parent_folder");
  std::string file_name("test_file_name.name");

  std::stringstream path("");
  path << default_download_path_ << "/"
       << "test_parent_folder"
       << "/"
       << "test_file_name.name";

  std::string expected = path.str();

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithParentFolder\
EndingWithBackslashAndFileNameArgumentsShouldReturnParentFolderAndFileName\
AppendedToBaseDownloadPath) {
  std::string parent_folder("test_parent_folder\\");
  std::string file_name("test_file_name.name");

  std::stringstream path("");
  path << default_download_path_ << "/"
       << "test_parent_folder"
       << "/"
       << "test_file_name.name";

  std::string expected = path.str();

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPathWithFileName\
StartingWithBackslashAndParentFolderArgumentsShouldReturnParentFolderAnd\
FileNameAppendedToBaseDownloadPath) {
  std::string parent_folder("test_parent_folder");
  std::string file_name("\\test_file_name.name");

  std::stringstream path("");
  path << default_download_path_ << "/"
       << "test_parent_folder"
       << "/"
       << "test_file_name.name";

  std::string expected = path.str();

  auto result =
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name);

  EXPECT_EQ(result, expected);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPath_FileDoesntExist\
ReturnsFileWithPassedName) {
  std::string file_name(kFileName);
  std::string parent_folder("");

  std::string expected(default_download_path_);
  expected.append(file_name.c_str());

  std::string actual(
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPath_FileExistsReturns\
FileWithIncrementedName) {
  std::string file_name(kFileName);
  std::string renamed_file_name(kFirstIterationFileName);
  std::string parent_folder("");

  std::string output_file_path(default_download_path_);
  output_file_path.append(file_name);

  std::string expected(default_download_path_);
  expected.append(renamed_file_name.c_str());

  std::ifstream input_file;
  std::ofstream output_file;

  output_file.open(output_file_path,
                   std::ofstream::binary | std::ofstream::out);

  ASSERT_TRUE(output_file.rdstate() == std::ofstream::goodbit);

  output_file.close();

  std::string actual(
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);

  // Remove the file and check that it is removed
  // File 1
  std::remove(output_file_path.c_str());

  input_file.open(output_file_path, std::ifstream::binary | std::ifstream::in);

  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPath_MultipleFilesExist\
ReturnsNextIncrementedFileName) {
  std::ofstream output_file;
  std::ifstream input_file;

  std::string file_name(kFileName);
  std::string first_renamed_file_name(kFirstIterationFileName);
  std::string second_renamed_file_name(kSecondIterationFileName);

  std::string parent_folder("");

  std::string expected(default_download_path_);
  expected.append(second_renamed_file_name.c_str());

  std::string output_file1_path(default_download_path_);
  output_file1_path.append(file_name);

  std::string output_file2_path(default_download_path_);
  output_file2_path.append(first_renamed_file_name);

  // Create the test files
  output_file.open(output_file1_path,
                   std::ofstream::binary | std::ofstream::out);
  ASSERT_TRUE(output_file.rdstate() == std::ofstream::goodbit);
  output_file.close();
  output_file.clear();

  output_file.open(output_file2_path,
                   std::ofstream::binary | std::ofstream::out);
  ASSERT_TRUE(output_file.rdstate() == std::ofstream::goodbit);
  output_file.close();

  std::string actual(
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(expected, actual);

  // Remove the test files and check that it is removed
  // File 1
  std::remove(output_file1_path.c_str());
  input_file.open(output_file1_path, std::ifstream::binary | std::ifstream::in);

  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);

  // File 2
  std::remove(output_file2_path.c_str());

  input_file.clear();
  input_file.open(output_file2_path, std::ifstream::binary | std::ifstream::in);

  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPath_FileNameContains\
MultipleDotsReturnsIncrementBeforeFirstDot) {
  std::ifstream input_file;
  std::ofstream output_file;

  std::string file_name(kMultipleDotsFileName);
  std::string renamed_file_name(kOneIterationMultipleDotsFileName);

  std::string parent_folder("");

  std::string output_file1_path(default_download_path_);
  output_file1_path.append(file_name);

  std::string output_file2_path(default_download_path_);
  output_file2_path.append(renamed_file_name);

  std::string expected(default_download_path_);
  expected.append(renamed_file_name);

  output_file.open(output_file1_path,
                   std::ofstream::binary | std::ofstream::out);
  ASSERT_TRUE(output_file.rdstate() == std::ofstream::goodbit);
  output_file.close();

  std::string actual(
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(expected, actual);

  std::remove(output_file1_path.c_str());
  input_file.open(output_file1_path, std::ifstream::binary | std::ifstream::in);

  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPath_FileNameContainsNo\
DotsReturnsWithIncrementAtEnd) {
  std::ifstream input_file;
  std::ofstream output_file;

  std::string file_name(kNoDotsFileName);
  std::string renamed_file_name(kOneIterationNoDotsFileName);

  std::string parent_folder("");

  std::string output_file1_path(default_download_path_);
  output_file1_path.append(file_name);

  std::string output_file2_path(default_download_path_);
  output_file2_path.append(renamed_file_name);

  std::string expected(default_download_path_);
  expected.append(renamed_file_name);

  output_file.open(output_file1_path,
                   std::ofstream::binary | std::ofstream::out);
  ASSERT_TRUE(output_file.rdstate() == std::ofstream::goodbit);
  output_file.close();

  std::string actual(
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(expected, actual);

  std::remove(output_file1_path.c_str());
  input_file.open(output_file1_path, std::ifstream::binary | std::ifstream::in);

  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);
}

TEST_F(ImplementationPlatformTests, DISABLED_GetDownloadPath_FileNameExistsWith\
AHoleBetweenRenamedFiles) {
  std::ifstream input_file;
  std::ofstream output_file;

  std::string file_name(kFileName);
  std::string file_name1(kFirstIterationFileName);
  std::string file_name2(kSecondIterationFileName);
  std::string file_name3(kThirdIterationFileName);

  std::string parent_folder("");

  // Create the path for the original file name
  std::string output_file_path(default_download_path_);
  output_file_path.append(
      file_name);  // Original file name example: "increment_file_test.txt"

  // Create the path for the first iteration of the original file name
  std::string output_file1_path(default_download_path_);
  output_file1_path.append(file_name1);  // First iteration on original file
                                         // name example:
                                         // "increment_file_test (1).txt"

  // Create the path for the third iteration of the original file name
  std::string output_file3_path(default_download_path_);
  output_file3_path.append(
      file_name3);  // Third iteration on original file
                    // name example: "increment_file_test (3).txt"

  // Create the expected result which is the second iteration of the original
  // file name
  std::string expected(default_download_path_);
  expected.append(file_name2);  // Second iteration on original file name
                                // example: "increment_file_test (2).txt"

  // Create the original file
  output_file.open(output_file_path,
                   std::ofstream::binary | std::ofstream::out);
  ASSERT_TRUE(output_file.rdstate() == std::ofstream::goodbit);
  output_file.close();

  // Create the first iteration of the original file
  output_file.clear();
  output_file.open(output_file1_path,
                   std::ofstream::binary | std::ofstream::out);
  ASSERT_TRUE(output_file.rdstate() == std::ofstream::goodbit);
  output_file.close();

  // Create the third iteration of the original file
  output_file.clear();
  output_file.open(output_file3_path,
                   std::ofstream::binary | std::ofstream::out);
  ASSERT_TRUE(output_file.rdstate() == std::ofstream::goodbit);
  output_file.close();

  // This should return the second iteration of the original file
  std::string actual(
      ImplementationPlatform::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(expected, actual);

  // Delete the original file
  std::remove(output_file_path.c_str());
  input_file.open(output_file_path, std::ifstream::binary | std::ifstream::in);
  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);

  // Delete the first iteration of the original file
  input_file.clear();  // Reset the input_file state
  std::remove(output_file1_path.c_str());
  input_file.open(output_file1_path, std::ifstream::binary | std::ifstream::in);
  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);

  // Delete the third iteration of the original file
  input_file.clear();  // Reset the input_file state
  std::remove(output_file3_path.c_str());
  input_file.open(output_file3_path, std::ifstream::binary | std::ifstream::in);
  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);
}
}  // namespace location::nearby::windows
