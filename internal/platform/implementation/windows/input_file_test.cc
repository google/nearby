// Copyright 2025 Google LLC
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

#include "internal/platform/implementation/windows/input_file.h"

#include <windows.h>

#include <cstddef>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/output_file.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/logging.h"

namespace nearby::windows {
namespace {

std::string GetTempFileNameForTest(const testing::TestInfo& test_info) {
  return absl::StrCat(testing::TempDir(), test_info.test_suite_name(), "_",
                      test_info.name());
}

std::string CreateTempFile(const testing::TestInfo& test_info, size_t size) {
  std::string temp_file = GetTempFileNameForTest(test_info);
  std::wstring wide_temp_file = string_utils::StringToWideString(temp_file);
  HANDLE file = ::CreateFileW(
      wide_temp_file.data(), GENERIC_WRITE, /*dwShareMode=*/0,
      /*lpSecurityAttributes=*/nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
      /*hTemplateFile=*/nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Failed to open temp file: " << temp_file
               << " with error: " << ::GetLastError();
    return "";
  }
  LARGE_INTEGER file_size;
  file_size.QuadPart = size;
  if (::SetFilePointerEx(file, file_size, /*lpNewFilePointer=*/nullptr,
                         FILE_BEGIN) == 0) {
    LOG(ERROR) << "Failed to set file pointer: " << temp_file
               << " with error: " << ::GetLastError();
    ::CloseHandle(file);
    return "";
  }
  if (::SetEndOfFile(file) == 0) {
    LOG(ERROR) << "Failed to set end of file: " << temp_file
               << " with error: " << ::GetLastError();
    ::CloseHandle(file);
    return "";
  }
  ::CloseHandle(file);
  return temp_file;
}

TEST(InputFileTest, NonexistentPathHasZeroSize) {
  InputFile input_file(/*file_path=*/"");

  EXPECT_EQ(input_file.GetTotalSize(), 0);
}

TEST(InputFileTest, NonexistentPathFailsToRead) {
  InputFile input_file(/*file_path=*/"");

  ExceptionOr<ByteArray> read_result = input_file.Read(/*size=*/1);

  EXPECT_FALSE(read_result.ok());
  EXPECT_EQ(read_result.exception(), Exception::kIo);
}

TEST(InputFileTest, NonexistentPathCloseSucceeds) {
  InputFile input_file(/*file_path=*/"");

  Exception close_result = input_file.Close();

  EXPECT_TRUE(close_result.Ok());
}

TEST(InputFileTest, LargeFileSize) {
  constexpr size_t kLargeFileSize = 3LL * 1024LL * 1024LL * 1024LL;
  std::string temp_file = CreateTempFile(
      *testing::UnitTest::GetInstance()->current_test_info(), kLargeFileSize);
  ASSERT_FALSE(temp_file.empty());

  InputFile input_file(temp_file);

  EXPECT_EQ(input_file.GetTotalSize(), kLargeFileSize);
  EXPECT_TRUE(input_file.Close().Ok());
  ::DeleteFileA(temp_file.data());
}

TEST(InputFileTest, ReadToEnd) {
  constexpr size_t kFileSize = 100;
  std::string temp_file = CreateTempFile(
      *testing::UnitTest::GetInstance()->current_test_info(), kFileSize);
  ASSERT_FALSE(temp_file.empty());

  InputFile input_file(temp_file);

  EXPECT_EQ(input_file.GetTotalSize(), kFileSize);
  ExceptionOr<ByteArray> read_result = input_file.Read(kFileSize);
  EXPECT_TRUE(read_result.ok());
  EXPECT_EQ(read_result.result().size(), kFileSize);

  read_result = input_file.Read(kFileSize);
  EXPECT_TRUE(read_result.ok());
  EXPECT_EQ(read_result.result().size(), 0);

  EXPECT_TRUE(input_file.Close().Ok());
  ::DeleteFileA(temp_file.data());
}

TEST(InputFileTest, GetLastModifiedTime) {
  std::string temp_file = GetTempFileNameForTest(
      *testing::UnitTest::GetInstance()->current_test_info());
  OutputFile output_file(temp_file);
  output_file.SetLastModifiedTime(absl::FromUnixSeconds(1234567890));
  EXPECT_TRUE(output_file.Close().Ok());

  InputFile input_file(temp_file);
  EXPECT_EQ(input_file.GetLastModifiedTime(),
            absl::FromUnixSeconds(1234567890));
  EXPECT_TRUE(input_file.Close().Ok());

  ::DeleteFileA(temp_file.data());
}

}  // namespace
}  // namespace nearby::windows
