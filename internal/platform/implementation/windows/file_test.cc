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

#include "internal/platform/implementation/windows/file.h"

#include <windows.h>

#include <cstddef>
#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/logging.h"

namespace nearby::windows {
namespace {

std::string GetTempDir() {
  std::string temp_path;
  temp_path.resize(MAX_PATH);
  if (::GetTempPathA(temp_path.size(), temp_path.data()) == 0) {
    LOG(ERROR) << "Failed to get temp path: " << ::GetLastError();
    return "";
  }
  return std::string(temp_path.data());
}

std::string GetTempFileName(absl::string_view prefix) {
  std::string temp_path = GetTempDir();
  if (temp_path.empty()) {
    return "";
  }
  std::string file_path;
  file_path.resize(MAX_PATH);
  UINT id = ::GetTempFileNameA(temp_path.data(), prefix.data(), /*uUnique=*/0,
                               file_path.data());
  if (id == 0) {
    LOG(ERROR) << "Failed to get temp file name with path: " << temp_path
               << ", error: " << ::GetLastError();
    return "";
  }
  return absl::StrCat(temp_path, prefix, id, ".tmp");
}

std::string CreateTempFile(absl::string_view prefix, size_t size) {
  std::string temp_file = GetTempFileName(prefix);
  if (temp_file.empty()) {
    return "";
  }
  HANDLE file = ::CreateFileA(
      temp_file.data(), GENERIC_WRITE, /*dwShareMode=*/0,
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
    return "";
  }
  if (::SetEndOfFile(file) == 0) {
    LOG(ERROR) << "Failed to set end of file: " << temp_file
               << " with error: " << ::GetLastError();
    return "";
  }
  ::CloseHandle(file);
  return temp_file;
}

TEST(IOFileTest, InputFileNonexistentPathHasZeroSize) {
  std::unique_ptr<IOFile> input_file =
      IOFile::CreateInputFile(/*file_path=*/"");
  ASSERT_NE(input_file, nullptr);

  EXPECT_EQ(input_file->GetTotalSize(), 0);
}

TEST(IOFileTest, InputFileNonexistentPathFailsToRead) {
  std::unique_ptr<IOFile> input_file =
      IOFile::CreateInputFile(/*file_path=*/"");
  ASSERT_NE(input_file, nullptr);

  ExceptionOr<ByteArray> read_result = input_file->Read(/*size=*/1);

  EXPECT_FALSE(read_result.ok());
  EXPECT_EQ(read_result.exception(), Exception::kIo);
}

TEST(IOFileTest, InputFileNonexistentPathCloseSucceeds) {
  std::unique_ptr<IOFile> input_file =
      IOFile::CreateInputFile(/*file_path=*/"");
  ASSERT_NE(input_file, nullptr);

  ExceptionOr<ByteArray> close_result = input_file->Close();

  EXPECT_TRUE(close_result.ok());
}

TEST(IOFileTest, InputFileLargeFileSize) {
  constexpr size_t kLargeFileSize = 3LL * 1024LL * 1024LL * 1024LL;
  std::string temp_file = CreateTempFile("LargeFileTest", kLargeFileSize);
  ASSERT_FALSE(temp_file.empty());

  std::unique_ptr<IOFile> input_file =
      IOFile::CreateInputFile(temp_file);
  ASSERT_NE(input_file, nullptr);

  EXPECT_EQ(input_file->GetTotalSize(), kLargeFileSize);
  ::DeleteFileA(temp_file.data());
}

TEST(IOFileTest, InputFileReadToEnd) {
  constexpr size_t kFileSize = 100;
  std::string temp_file = CreateTempFile("ReadToEnd", kFileSize);
  ASSERT_FALSE(temp_file.empty());

  std::unique_ptr<IOFile> input_file =
      IOFile::CreateInputFile(temp_file);
  ASSERT_NE(input_file, nullptr);

  EXPECT_EQ(input_file->GetTotalSize(), kFileSize);
  ExceptionOr<ByteArray> read_result = input_file->Read(kFileSize);
  EXPECT_TRUE(read_result.ok());
  EXPECT_EQ(read_result.result().size(), kFileSize);

  read_result = input_file->Read(kFileSize);
  EXPECT_TRUE(read_result.ok());
  EXPECT_EQ(read_result.result().size(), 0);

  ::DeleteFileA(temp_file.data());
}

TEST(IOFileTest, OutputFileAlreadyExists) {
  constexpr size_t kFileSize = 100;
  std::string temp_file = CreateTempFile("OutputFileExists", kFileSize);
  ASSERT_FALSE(temp_file.empty());

  std::unique_ptr<IOFile> output_file = IOFile::CreateOutputFile(temp_file);
  ASSERT_NE(output_file, nullptr);

  EXPECT_EQ(output_file->GetTotalSize(), 0);
  ExceptionOr<ByteArray> write_result = output_file->Write(ByteArray("test"));
  EXPECT_FALSE(write_result.ok());
  EXPECT_TRUE(write_result.GetException().Raised(Exception::kIo));

  ::DeleteFileA(temp_file.data());
}

TEST(IOFileTest, OutputFileWrite) {
  std::string temp_file = GetTempFileName("WriteFileTest");
  std::unique_ptr<IOFile> output_file = IOFile::CreateOutputFile(temp_file);
  ASSERT_NE(output_file, nullptr);

  ExceptionOr<ByteArray> write_result = output_file->Write(ByteArray("test1"));
  EXPECT_TRUE(write_result.ok());
  write_result = output_file->Write(ByteArray("test2"));
  EXPECT_TRUE(write_result.ok());
  EXPECT_TRUE(output_file->Close().Ok());

  std::unique_ptr<IOFile> input_file =
      IOFile::CreateInputFile(temp_file);
  ExceptionOr<ByteArray> read_result = input_file->Read(10);
  EXPECT_TRUE(read_result.ok());
  EXPECT_EQ(read_result.result(), ByteArray("test1test2"));

  ::DeleteFileA(temp_file.data());
}

TEST(IOFileTest, GetSetModifiedTime) {
  std::string temp_file = GetTempFileName("GetSetModifiedTime");
  std::unique_ptr<IOFile> output_file = IOFile::CreateOutputFile(temp_file);
  ASSERT_NE(output_file, nullptr);

  output_file->SetLastModifiedTime(absl::FromUnixSeconds(1234567890));
  output_file->Close();

  std::unique_ptr<IOFile> input_file =
      IOFile::CreateInputFile(temp_file);
  EXPECT_EQ(input_file->GetLastModifiedTime(),
            absl::FromUnixSeconds(1234567890));

  ::DeleteFileA(temp_file.data());
}

}  // namespace
}  // namespace nearby::windows
