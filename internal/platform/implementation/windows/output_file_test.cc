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

#include "internal/platform/implementation/windows/output_file.h"

#include <windows.h>

#include <cstddef>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/input_file.h"
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

TEST(OutputFileTest, AlreadyExists) {
  constexpr size_t kFileSize = 100;
  std::string temp_file = CreateTempFile(
      *testing::UnitTest::GetInstance()->current_test_info(), kFileSize);
  ASSERT_FALSE(temp_file.empty());

  OutputFile output_file(temp_file);

  Exception write_result = output_file.Write("test");
  EXPECT_FALSE(write_result.Ok());
  EXPECT_TRUE(write_result.Raised(Exception::kIo));

  ::DeleteFileA(temp_file.data());
}

TEST(OutputFileTest, Write) {
  std::string temp_file = GetTempFileNameForTest(
      *testing::UnitTest::GetInstance()->current_test_info());
  OutputFile output_file(temp_file);

  Exception write_result = output_file.Write("test1");
  EXPECT_TRUE(write_result.Ok());
  write_result = output_file.Write("test2");
  EXPECT_TRUE(write_result.Ok());
  EXPECT_TRUE(output_file.Close().Ok());

  InputFile input_file(temp_file);
  ExceptionOr<ByteArray> read_result = input_file.Read(10);
  EXPECT_TRUE(read_result.ok());
  EXPECT_EQ(read_result.result(), ByteArray("test1test2"));
  EXPECT_TRUE(input_file.Close().Ok());

  ::DeleteFileA(temp_file.data());
}

TEST(OutputFileTest, GetSetModifiedTime) {
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

TEST(OutputFileTest, SetOriginMetadataAddsZoneIdentifierStream) {
  static constexpr absl::string_view kZoneIdentifierContent =
      "[ZoneTransfer]\r\nZoneId=3\r\n";
  std::string temp_file = GetTempFileNameForTest(
      *testing::UnitTest::GetInstance()->current_test_info());
  OutputFile output_file(temp_file);
  EXPECT_TRUE(output_file.Write("test").Ok());

  EXPECT_TRUE(output_file.SetOriginMetadata());
  EXPECT_TRUE(output_file.Close().Ok());

  std::wstring ads_file_path =
      string_utils::StringToWideString(temp_file).append(L":Zone.Identifier");

  HANDLE read_handle = ::CreateFileW(
      ads_file_path.c_str(), GENERIC_READ,
      /*dwShareMode=*/0, /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, /*hTemplateFile=*/nullptr);
  ASSERT_NE(read_handle, INVALID_HANDLE_VALUE);
  char buffer[100];
  DWORD bytes_read = 0;
  EXPECT_NE(::ReadFile(read_handle, buffer, sizeof(buffer), &bytes_read,
                       /*lpOverlapped=*/nullptr),
            0);
  EXPECT_EQ(std::string(buffer, bytes_read), kZoneIdentifierContent);
  ::CloseHandle(read_handle);
  ::DeleteFileA(temp_file.data());
}

TEST(OutputFileTest, SetOriginMetadataAfterCloseDoesNothing) {
  std::string temp_file = GetTempFileNameForTest(
      *testing::UnitTest::GetInstance()->current_test_info());
  OutputFile output_file(temp_file);
  EXPECT_TRUE(output_file.Write("test").Ok());
  EXPECT_TRUE(output_file.Close().Ok());

  EXPECT_FALSE(output_file.SetOriginMetadata());

  std::wstring ads_file_path =
      string_utils::StringToWideString(temp_file).append(L":Zone.Identifier");

  HANDLE read_handle = ::CreateFileW(
      ads_file_path.c_str(), GENERIC_READ,
      /*dwShareMode=*/0, /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, /*hTemplateFile=*/nullptr);
  EXPECT_EQ(read_handle, INVALID_HANDLE_VALUE);
  ::DeleteFileA(temp_file.data());
}

}  // namespace
}  // namespace nearby::windows
