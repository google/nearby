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

#include "internal/platform/implementation/windows/get_custom_save_path.h"

// clang-format off
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>
// clang-format on

#include <fstream>
#include <initializer_list>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"

namespace nearby::windows {

namespace {
const absl::string_view kIllegalPathNames[] = {
    "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
    "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
    "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

const absl::string_view kFileName("increment_file_test.txt");
const absl::string_view kFirstIterationFileName("increment_file_test (1).txt");
const absl::string_view kSecondIterationFileName("increment_file_test (2).txt");
}  // namespace

// Can't run on google 3, I presume the SHGetKnownFolderPath
// fails.
class FilePathTests : public testing::Test {};

TEST_F(FilePathTests, GetCustomSavePathUnchanged) {
  FilePath path("C:/test_parent_folder/test_file_name.name");

  FilePath actual(GetCustomSavePath(path));

  EXPECT_EQ(actual, path);
}

TEST_F(FilePathTests, GetCustomSavePathIllegalPathComponentReturnsUnderbar) {
  for (auto illegal_path_name : kIllegalPathNames) {
    FilePath parent_folder("C:\\TEMP");

    FilePath expected(parent_folder);
    expected.append(FilePath(absl::StrCat("_", illegal_path_name))
                        .append(FilePath(kFileName)));

    FilePath actual(
        GetCustomSavePath(parent_folder.append(FilePath(illegal_path_name))
                              .append(FilePath(kFileName))));

    EXPECT_EQ(actual, expected);
  }
}

TEST_F(FilePathTests,
       GetCustomSavePathIllegalPathComponentLowerCaseReturnsUnderbar) {
  for (auto illegal_path_name : kIllegalPathNames) {
    FilePath parent_folder("C:\\TEMP");

    FilePath expected(parent_folder);
    expected.append(
        FilePath(absl::StrCat("_", absl::AsciiStrToLower(illegal_path_name)))
            .append(FilePath(kFileName)));

    FilePath actual(GetCustomSavePath(
        parent_folder.append(FilePath(absl::AsciiStrToLower(illegal_path_name)))
            .append(FilePath(kFileName))));

    EXPECT_EQ(actual, expected);
  }
}

TEST_F(FilePathTests, GetCustomSavePathDotPathComponentReturnsUnderbar) {
  FilePath path("C:/test_parent_folder/./test_file_name.name");

  FilePath actual(GetCustomSavePath(path));

  EXPECT_EQ(actual, FilePath("C:/test_parent_folder/_/test_file_name.name"));
}

TEST_F(FilePathTests, GetCustomSavePathIllegalCharactersReturnsUnderbar) {
  // char illegal_character_sequence[]{ 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x05,
  // 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0 };
  for (absl::string_view illegal_character :
       std::initializer_list<absl::string_view>{absl::string_view("\0", 1),
                                                "\x1", "\x1f", "?", "*", "<",
                                                ">", "|", ":", "\""}) {
    FilePath illegal_character_sequence(
        absl::StrCat("C:/Test", illegal_character, "Test"));

    FilePath actual(GetCustomSavePath(illegal_character_sequence));

    EXPECT_EQ(actual, FilePath("C:/Test_Test"));
  }
}

TEST_F(FilePathTests, GetCustomSavePathFileExistsReturnsIncrementedName) {
  FilePath temp_path = Files::GetTemporaryDirectory();
  FilePath file_name = temp_path;
  file_name.append(FilePath(kFileName));
  FilePath renamed_file_name = temp_path;
  renamed_file_name.append(FilePath(kFirstIterationFileName));
  Files::RemoveFile(renamed_file_name);
  std::wofstream output_file;
  output_file.open(file_name.GetPath(),
                   std::ofstream::binary | std::ofstream::out);
  ASSERT_TRUE(output_file.rdstate() == std::ofstream::goodbit);
  output_file.close();

  FilePath actual(GetCustomSavePath(file_name));

  EXPECT_EQ(actual, renamed_file_name);
}

TEST_F(FilePathTests,
       GetCustomSavePathMultipleFilesExistReturnsNextIncremented) {
  FilePath temp_path = Files::GetTemporaryDirectory();
  FilePath file_name = temp_path;
  file_name.append(FilePath(kFileName));
  FilePath renamed_file_name1 = temp_path;
  renamed_file_name1.append(FilePath(kFirstIterationFileName));
  FilePath renamed_file_name2 = temp_path;
  renamed_file_name2.append(FilePath(kSecondIterationFileName));
  Files::RemoveFile(renamed_file_name2);
  std::wofstream output_file;
  output_file.open(file_name.GetPath(),
                   std::ofstream::binary | std::ofstream::out);
  ASSERT_TRUE(output_file.rdstate() == std::ofstream::goodbit);
  output_file.close();
  std::wofstream output_file1;
  output_file.open(renamed_file_name1.GetPath(),
                   std::ofstream::binary | std::ofstream::out);
  ASSERT_TRUE(output_file1.rdstate() == std::ofstream::goodbit);
  output_file1.close();

  FilePath actual(GetCustomSavePath(file_name));

  EXPECT_EQ(actual, renamed_file_name2);
}

}  // namespace nearby::windows
