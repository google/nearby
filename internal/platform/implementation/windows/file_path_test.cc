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

#include "internal/platform/implementation/windows/file_path.h"

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace nearby {
namespace windows {

namespace {
const wchar_t* kIllegalPathNames[] = {
    L"CON",  L"PRN",  L"AUX",  L"NUL",  L"COM1", L"COM2", L"COM3", L"COM4",
    L"COM5", L"COM6", L"COM7", L"COM8", L"COM9", L"LPT1", L"LPT2", L"LPT3",
    L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9"};

const wchar_t* kFileName(L"increment_file_test.txt");
const wchar_t* kFirstIterationFileName(L"/increment_file_test (1).txt");
const wchar_t* kSecondIterationFileName(L"/increment_file_test (2).txt");
const wchar_t* kThirdIterationFileName(L"/increment_file_test (3).txt");
const wchar_t* kNoDotsFileName(L"incrementfiletesttxt");
const wchar_t* kOneIterationNoDotsFileName(L"/incrementfiletesttxt (1)");
const wchar_t* kMultipleDotsFileName(L"increment.file.test.txt");
const wchar_t* kOneIterationMultipleDotsFileName(
    L"/increment.file.test (1).txt");
const wchar_t* kImmediateEscape(L"../");
const wchar_t* kLongEscapeBackSlash(L"..\\test\\..\\..\\test");
const wchar_t* kTwoLevelFolder(L"/test/test");
const wchar_t* kLongEscapeSlash(L"../test/../../test");
const wchar_t* kLongEscapeMixedSlash(L"../test\\..\\../test");
const wchar_t* kLongEscapeEndingEscape(L"../test/../../test/..");
const wchar_t* kLongEscapeEndingEscapeWithSlash(
    L"../test/../../test/../../../");
}  // namespace

// Can't run on google 3, I presume the SHGetKnownFolderPath
// fails.
class FilePathTests : public testing::Test {
 protected:
  // You can define per-test set-up logic as usual.
  FilePathTests() {
    PWSTR basePath;

    SHGetKnownFolderPath(
        FOLDERID_Downloads,  //  rfid: A reference to the KNOWNFOLDERID that
                             //        identifies the folder.
        0,                   // dwFlags: Flags that specify special retrieval
                             //          options.
        nullptr,             // hToken: An access token that represents a
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

    // size_t bufferSize;
    // wcstombs_s(&bufferSize, nullptr, 0, basePath, 0);
    // default_download_path_.resize(bufferSize - 1, '\0');
    // wcstombs_s(&bufferSize, default_download_path_.data(), bufferSize,
    // basePath,
    //            _TRUNCATE);
    default_download_path_ = basePath;

    std::replace(default_download_path_.begin(), default_download_path_.end(),
                 L'\\', L'/');
  }

  std::wstring default_download_path_;
};

TEST_F(FilePathTests, GetDownloadPathWithEmptyStringArguments\
ShouldReturnBaseDownloadPath) {
  std::wstring parent_folder(L"");
  std::wstring file_name(L"");

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, default_download_path_);
}  // NOLINT false lint error here

TEST_F(FilePathTests, GetDownloadPathWithSlashParent\
FolderArgumentsShouldReturnBaseDownloadPath) {
  std::wstring parent_folder(L"/");
  std::wstring file_name(L"");

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, default_download_path_);
}  // NOLINT false lint error here

TEST_F(FilePathTests, GetDownloadPathWithBackslashParent\
FolderArgumentsShouldReturnBaseDownloadPath) {
  std::wstring parent_folder(L"\\");
  std::wstring file_name(L"");

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, default_download_path_);
}  // NOLINT false lint error here

TEST_F(FilePathTests, GetDownloadPathWithAttemptToEscape\
UsersDownloadFolderShouldReturnDownloadPathNotEscapingUsersDownloadFolder) {
  std::wstring parent_folder(kImmediateEscape);
  std::wstring file_name(L"");

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, default_download_path_);
}

TEST_F(FilePathTests, GetDownloadPathWithMultiple\
AttemptsToEscapeUsersDownloadFolderWithBackslashShouldReturnDownloadPath\
NotEscapingUsersDownloadFolder) {
  std::wstring parent_folder(kLongEscapeBackSlash);
  std::wstring file_name(L"");

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, default_download_path_ + kTwoLevelFolder);
}

TEST_F(FilePathTests, GetDownloadPathWithMultiple\
AttemptsToEscapeUsersDownloadFolderShouldReturnDownloadPathNotEscapingUsers\
DownloadFolder) {
  std::wstring parent_folder(kLongEscapeSlash);
  std::wstring file_name(L"");

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, default_download_path_ + kTwoLevelFolder);
}

TEST_F(FilePathTests, GetDownloadPathWithMultiple\
AttemptsToEscapeUsersDownloadFolderWithMixedSlashShouldReturnDownloadPath\
NotEscapingUsersDownloadFolder) {
  std::wstring parent_folder(kLongEscapeMixedSlash);
  std::wstring file_name(L"");

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, default_download_path_ + kTwoLevelFolder);
}

TEST_F(FilePathTests, GetDownloadPathWithMultiple\
AttemptsToEscapeUsersDownloadFolderWithEndingEscapeShouldReturnDownload\
PathNotEscapingUsersDownloadFolder) {
  std::wstring parent_folder(kLongEscapeEndingEscape);
  std::wstring file_name(L"");

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, default_download_path_ + kTwoLevelFolder);
}

TEST_F(FilePathTests, GetDownloadPathWithMultiple\
AttemptsToEscapeUsersDownloadFolderWithEndingSlashShouldReturnDownloadPathNot\
EscapingUsersDownloadFolder) {
  std::wstring parent_folder(kLongEscapeEndingEscapeWithSlash);
  std::wstring file_name(L"");

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, default_download_path_ + kTwoLevelFolder);
}

TEST_F(FilePathTests, GetDownloadPathWithSlashFileName\
ArgumentsShouldReturnBaseDownloadPath) {
  std::wstring parent_folder(L"");
  std::wstring file_name(L"/");

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, default_download_path_);
}

TEST_F(FilePathTests, GetDownloadPathWithBackslashFile\
NameArgumentsShouldReturnBaseDownloadPath) {
  std::wstring parent_folder(L"");
  std::wstring file_name(L"\\");

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  auto result_size = actual.size();
  auto default_size = default_download_path_.size();

  EXPECT_EQ(actual, default_download_path_);
}

TEST_F(FilePathTests, GetDownloadPathWithParentFolder\
ShouldReturnParentFolderAppendedToBaseDownloadPath) {
  std::wstring parent_folder(L"test_parent_folder");
  std::wstring file_name(L"");

  std::wstringstream path(L"");
  path << default_download_path_ << L"/" << "test_parent_folder";

  std::wstring expected = path.str();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPathWithParentFolder\
StartingWithSlashArgumentsShouldReturnParentFolderAppendedToBaseDownloadPath) {
  std::wstring parent_folder(L"/test_parent_folder");
  std::wstring file_name(L"");

  std::wstringstream path(L"");
  path << default_download_path_ << L"/" << "test_parent_folder";

  std::wstring expected = path.str();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPathWithParentFolder\
StartingWithBackslashArgumentsShouldReturnParentFolderAppendedToBase\
DownloadPath) {
  std::wstring parent_folder(L"\\test_parent_folder");
  std::wstring file_name(L"");

  std::wstringstream path(L"");
  path << default_download_path_ << L"/" << "test_parent_folder";

  std::wstring expected = path.str();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPathWithParentFolder\
EndingWithSlashArgumentsShouldReturnParentFolderAppendedToBaseDownloadPath) {
  std::wstring parent_folder(L"test_parent_folder/");
  std::wstring file_name(L"");

  std::wstringstream path(L"");
  path << default_download_path_ << L"/" << "test_parent_folder";

  std::wstring expected = path.str();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPathWithParentFolder\
EndingWithBackslashArguments\
ShouldReturnParentFolderAppendedToBaseDownloadPath) {
  std::wstring parent_folder(L"test_parent_folder\\");
  std::wstring file_name(L"");

  std::wstringstream path(L"");
  path << default_download_path_ << L"/" << "test_parent_folder";

  std::wstring expected = path.str();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPathWithFileName\
BeginningWithSlashArgumentsShouldReturnFileNameAppendedToBaseDownloadPath) {
  std::wstring parent_folder(L"");
  std::wstring file_name(L"/test_file_name.name");

  std::wstringstream path(L"");
  path << default_download_path_ << L"/" << "test_file_name.name";

  std::wstring expected = path.str();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPathWithFileName\
BeginningWithBackslashArgumentsShouldReturnFileNameAppendedToBaseDownloadPath) {
  std::wstring parent_folder(L"");
  std::wstring file_name(L"\\test_file_name.name");

  std::wstringstream path(L"");
  path << default_download_path_ << L"/" << "test_file_name.name";

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, path.str().c_str());
}

TEST_F(FilePathTests, GetDownloadPathWithFileNameEnding\
WithSlashArgumentsShouldReturnFileNameAppendedToBaseDownloadPath) {
  std::wstring parent_folder(L"");
  std::wstring file_name(L"test_file_name.name/");

  std::wstringstream path(L"");
  path << default_download_path_ << L"/" << "test_file_name.name";

  std::wstring expected = path.str();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPathWithFileNameEnding\
WithBackslashArgumentsShouldReturnFileNameAppendedToBaseDownloadPath) {
  std::wstring parent_folder(L"");
  std::wstring file_name(L"test_file_name.name\\");

  std::wstringstream path(L"");
  path << default_download_path_ << L"/" << "test_file_name.name";

  std::wstring expected = path.str();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPathWithParentFolderAnd\
FileNameArgumentsShould\
ReturnParentFolderAndFileNameAppendedToBaseDownloadPath) {
  std::wstring parent_folder(L"test_parent_folder");
  std::wstring file_name(L"test_file_name.name");

  std::wstringstream path(L"");
  path << default_download_path_ << L"/" << "test_parent_folder"
       << "/"
       << "test_file_name.name";

  std::wstring expected = path.str();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPathWithParentFolder\
EndingWithBackslashAndFileNameArgumentsShouldReturnParentFolderAndFileName\
AppendedToBaseDownloadPath) {
  std::wstring parent_folder(L"test_parent_folder\\");
  std::wstring file_name(L"test_file_name.name");

  std::wstringstream path(L"");
  path << default_download_path_ << L"/" << "test_parent_folder"
       << "/"
       << "test_file_name.name";

  std::wstring expected = path.str();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPathWithFileName\
StartingWithBackslashAndParentFolderArgumentsShouldReturnParentFolderAnd\
FileNameAppendedToBaseDownloadPath) {
  std::wstring parent_folder(L"test_parent_folder");
  std::wstring file_name(L"\\test_file_name.name");

  std::wstringstream path(L"");
  path << default_download_path_ << L"/" << "test_parent_folder"
       << "/"
       << "test_file_name.name";

  std::wstring expected = path.str();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPath_IllegalPathComponent\
ReturnsComponentWithUnderbarPrepended) {
  for (auto illegal_file_name : kIllegalPathNames) {
    std::wstring parent_folder(L"");

    std::wstring expected(default_download_path_);
    expected.append(L"/_" + std::wstring(illegal_file_name) +
                    std::wstring(L"/") + std::wstring(kFileName));

    auto actual(FilePath::GetDownloadPath(illegal_file_name, kFileName));

    EXPECT_EQ(actual, expected);
  }
}

TEST_F(FilePathTests, GetDownloadPath_IllegalPathComponentInLowerCase\
ReturnsComponentWithUnderbarPrepended) {
  for (auto illegal_file_name : kIllegalPathNames) {
    std::wstring parent_folder(L"");

    std::wstring expected(default_download_path_);

    expected.append(L"/_" + std::wstring(_wcslwr(
                                (std::wstring(illegal_file_name)).data())));
    expected.append(L"/" + std::wstring(kFileName));

    auto actual(FilePath::GetDownloadPath(
        std::wstring(_wcslwr((std::wstring(illegal_file_name)).data())),
        kFileName));

    EXPECT_EQ(actual, expected);
  }
}

TEST_F(FilePathTests, GetDownloadPath_IllegalFileNameInLowerCase\
ReturnsFileNameWithUnderbarPrepended) {
  for (auto illegal_file_name : kIllegalPathNames) {
    std::wstring parent_folder(L"");

    std::wstring expected(default_download_path_);
    expected.append(L"/_" + std::wstring(_wcslwr(
                                (std::wstring(illegal_file_name)).data())));

    auto actual(FilePath::GetDownloadPath(
        parent_folder,
        std::wstring(_wcslwr((std::wstring(illegal_file_name)).data()))));

    EXPECT_EQ(actual, expected);
  }
}

TEST_F(FilePathTests, GetDownloadPath_IllegalFileNameCharacters\
ReturnsFileNameWithUnderbarSubstituted) {
  // char illegal_character_sequence[]{ 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x05,
  // 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0 };
  auto illegal_character_sequence(L"Test\x5Test");
  std::wstring parent_folder(L"");

  std::wstring expected(default_download_path_);
  expected.append(L"/Test_Test");

  auto actual(FilePath::GetDownloadPath(
      parent_folder, std::wstring(illegal_character_sequence)));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPath_LowestIllegalFileNameCharacter\
ReturnsFileNameWithUnderbarSubstituted) {
  // char illegal_character_sequence[]{ 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x01,
  // 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0 };
  auto illegal_character_sequence(L"Test\x1Test");

  std::wstring parent_folder(L"");

  std::wstring expected(default_download_path_);
  expected.append(L"/Test_Test");

  auto actual(FilePath::GetDownloadPath(
      parent_folder, std::wstring(illegal_character_sequence)));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPath_HighestIllegalFileNameCharacter\
ReturnsFileNameWithUnderbarSubstituted) {
  // char illegal_character_sequence[]{ 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x1f,
  // 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0 };
  auto illegal_character_sequence(L"Test\x1fTest");

  std::wstring parent_folder(L"");

  std::wstring expected(default_download_path_);
  expected.append(L"/Test_Test");

  auto actual(FilePath::GetDownloadPath(
      parent_folder, std::wstring(illegal_character_sequence)));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPath_IllegalFileNameCharacterQuestionMark\
ReturnsFileNameWithUnderbarSubstituted) {
  // char illegal_character_sequence[]{ 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2f,
  // 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0 };
  auto illegal_character_sequence(L"Test?Test");

  std::wstring parent_folder(L"");

  std::wstring expected(default_download_path_);
  expected.append(L"/Test_Test");

  auto actual(FilePath::GetDownloadPath(
      parent_folder, std::wstring(illegal_character_sequence)));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPath_IllegalFileNameCharacterAsterisk\
ReturnsFileNameWithUnderbarSubstituted) {
  // char illegal_character_sequence[]{ 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2f,
  // 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0 };
  auto illegal_character_sequence(L"Test*Test");

  std::wstring parent_folder(L"");

  std::wstring expected(default_download_path_);
  expected.append(L"/Test_Test");

  auto actual(FilePath::GetDownloadPath(
      parent_folder, std::wstring(illegal_character_sequence)));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPath_IllegalFileNameCharacterLessThan\
ReturnsFileNameWithUnderbarSubstituted) {
  // char illegal_character_sequence[]{ 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2f,
  // 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0 };
  auto illegal_character_sequence(L"Test<Test");

  std::wstring parent_folder(L"");

  std::wstring expected(default_download_path_);
  expected.append(L"/Test_Test");

  auto actual(FilePath::GetDownloadPath(
      parent_folder, std::wstring(illegal_character_sequence)));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPath_IllegalFileNameCharacterGreaterThan\
ReturnsFileNameWithUnderbarSubstituted) {
  // char illegal_character_sequence[]{ 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2f,
  // 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0 };
  auto illegal_character_sequence(L"Test>Test");

  std::wstring parent_folder(L"");

  std::wstring expected(default_download_path_);
  expected.append(L"/Test_Test");

  auto actual(FilePath::GetDownloadPath(
      parent_folder, std::wstring(illegal_character_sequence)));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPath_IllegalFileNameCharacterVerticalBar\
ReturnsFileNameWithUnderbarSubstituted) {
  // char illegal_character_sequence[]{ 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2f,
  // 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0 };
  auto illegal_character_sequence(L"Test|Test");

  std::wstring parent_folder(L"");

  std::wstring expected(default_download_path_);
  expected.append(L"/Test_Test");

  auto actual(FilePath::GetDownloadPath(
      parent_folder, std::wstring(illegal_character_sequence)));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPath_IllegalFileNameCharacterColon\
ReturnsFileNameWithUnderbarSubstituted) {
  // char illegal_character_sequence[]{ 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2f,
  // 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0 };
  auto illegal_character_sequence(L"Test:Test");

  std::wstring parent_folder(L"");

  std::wstring expected(default_download_path_);
  expected.append(L"/Test_Test");

  auto actual(FilePath::GetDownloadPath(
      parent_folder, std::wstring(illegal_character_sequence)));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPath_FileDoesntExist\
ReturnsFileWithPassedName) {
  std::wstring file_name(kFileName);
  std::wstring parent_folder(L"");

  std::wstring expected(default_download_path_);
  expected.append(L"/");
  expected.append(file_name);

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);
}

TEST_F(FilePathTests, GetDownloadPath_FileExistsReturns\
FileWithIncrementedName) {
  std::wstring file_name(kFileName);
  std::wstring renamed_file_name(kFirstIterationFileName);
  std::wstring parent_folder(L"");

  std::wstring output_file_path(default_download_path_);
  output_file_path.append(L"/");
  output_file_path.append(file_name);

  std::wstring expected(default_download_path_);
  expected += renamed_file_name;

  std::wifstream input_file;
  std::wofstream output_file;

  output_file.open(output_file_path,
                   std::ofstream::binary | std::ofstream::out);

  ASSERT_TRUE(output_file.rdstate() == std::ofstream::goodbit);

  output_file.close();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(actual, expected);

  // Remove the file and check that it is removed
  // File 1
  _wremove(output_file_path.c_str());

  input_file.open(output_file_path, std::ifstream::binary | std::ifstream::in);

  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);
}

TEST_F(FilePathTests, GetDownloadPath_MultipleFilesExist\
ReturnsNextIncrementedFileName) {
  std::ofstream output_file;
  std::ifstream input_file;

  std::wstring file_name(kFileName);
  std::wstring first_renamed_file_name(kFirstIterationFileName);
  std::wstring second_renamed_file_name(kSecondIterationFileName);

  std::wstring parent_folder(L"");

  std::wstring expected(default_download_path_);
  expected.append(second_renamed_file_name.c_str());

  std::wstring output_file1_path(default_download_path_);
  output_file1_path.append(L"/" + file_name);

  std::wstring output_file2_path(default_download_path_);
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

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(expected, actual);

  // Remove the test files and check that it is removed
  // File 1
  _wremove(output_file1_path.c_str());
  input_file.open(output_file1_path, std::ifstream::binary | std::ifstream::in);

  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);

  // File 2
  _wremove(output_file2_path.c_str());

  input_file.clear();
  input_file.open(output_file2_path, std::ifstream::binary | std::ifstream::in);

  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);
}

TEST_F(FilePathTests, GetDownloadPath_FileNameContains\
MultipleDotsReturnsIncrementBeforeFirstDot) {
  std::ifstream input_file;
  std::ofstream output_file;

  std::wstring file_name(kMultipleDotsFileName);
  std::wstring renamed_file_name(kOneIterationMultipleDotsFileName);

  std::wstring parent_folder(L"");

  std::wstring output_file1_path(default_download_path_);
  output_file1_path.append(L"/" + file_name);

  std::wstring output_file2_path(default_download_path_);
  output_file2_path.append(renamed_file_name);

  std::wstring expected(default_download_path_);
  expected.append(renamed_file_name);

  output_file.open(output_file1_path,
                   std::ofstream::binary | std::ofstream::out);
  ASSERT_TRUE(output_file.rdstate() == std::ofstream::goodbit);
  output_file.close();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(expected, actual);

  _wremove(output_file1_path.c_str());
  input_file.open(output_file1_path, std::ifstream::binary | std::ifstream::in);

  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);
}

TEST_F(FilePathTests, GetDownloadPath_FileNameContainsNo\
DotsReturnsWithIncrementAtEnd) {
  std::ifstream input_file;
  std::ofstream output_file;

  std::wstring file_name(kNoDotsFileName);
  std::wstring renamed_file_name(kOneIterationNoDotsFileName);

  std::wstring parent_folder(L"");

  std::wstring output_file1_path(default_download_path_);
  output_file1_path.append(L"/" + file_name);

  std::wstring output_file2_path(default_download_path_);
  output_file2_path.append(L"/" + renamed_file_name);

  std::wstring expected(default_download_path_);
  expected.append(renamed_file_name);

  output_file.open(output_file1_path,
                   std::ofstream::binary | std::ofstream::out);
  ASSERT_TRUE(output_file.rdstate() == std::ofstream::goodbit);
  output_file.close();

  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(expected, actual);

  _wremove(output_file1_path.c_str());
  input_file.open(output_file1_path, std::ifstream::binary | std::ifstream::in);

  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);
}

TEST_F(FilePathTests, GetDownloadPath_FileNameExistsWith\
AHoleBetweenRenamedFiles) {
  std::ifstream input_file;
  std::ofstream output_file;

  std::wstring file_name(kFileName);
  std::wstring file_name1(kFirstIterationFileName);
  std::wstring file_name2(kSecondIterationFileName);
  std::wstring file_name3(kThirdIterationFileName);

  std::wstring parent_folder(L"");

  // Create the path for the original file name
  std::wstring output_file_path(default_download_path_);
  output_file_path.append(
      L"/" +
      file_name);  // Original file name example: "increment_file_test.txt"

  // Create the path for the first iteration of the original file name
  std::wstring output_file1_path(default_download_path_);
  output_file1_path.append(file_name1);  // First iteration on original file
                                         // name example:
                                         // "increment_file_test (1).txt"

  // Create the path for the third iteration of the original file name
  std::wstring output_file3_path(default_download_path_);
  output_file3_path.append(
      file_name3);  // Third iteration on original file
                    // name example: "increment_file_test (3).txt"

  // Create the expected result which is the second iteration of the original
  // file name
  std::wstring expected(default_download_path_);
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
  auto actual(FilePath::GetDownloadPath(parent_folder, file_name));

  EXPECT_EQ(expected, actual);

  // Delete the original file
  _wremove(output_file_path.c_str());
  input_file.open(output_file_path, std::ifstream::binary | std::ifstream::in);
  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);

  // Delete the first iteration of the original file
  input_file.clear();  // Reset the input_file state
  _wremove(output_file1_path.c_str());
  input_file.open(output_file1_path, std::ifstream::binary | std::ifstream::in);
  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);

  // Delete the third iteration of the original file
  input_file.clear();  // Reset the input_file state
  _wremove(output_file3_path.c_str());
  input_file.open(output_file3_path, std::ifstream::binary | std::ifstream::in);
  ASSERT_FALSE(input_file.rdstate() == std::ifstream::goodbit);
}
}  // namespace windows
}  // namespace nearby
