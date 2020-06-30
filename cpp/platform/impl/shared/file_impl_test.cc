// Copyright 2020 Google LLC
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

#include "platform/impl/shared/file_impl.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <ostream>
#include <unistd.h>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {


class FileImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    path_ = std::tmpnam(nullptr);;
    std::ofstream output_file(path_);
    file_ = std::fstream(path_, std::fstream::in | std::fstream::out);
  }

  void WriteToFile(absl::string_view text) {
    file_ << text;
    file_.flush();
    size_ += text.size();
  }

  size_t GetSize() const { return size_; }

  void AssertEquals(const ExceptionOr<ConstPtr<ByteArray>>& bytes,
                    const std::string& expected) {
    ASSERT_TRUE(bytes.ok());
    ScopedPtr<ConstPtr<ByteArray>> byte_array(bytes.result());
    ASSERT_STREQ(byte_array->getData(), expected.c_str());
    ASSERT_EQ(byte_array->size(), expected.length());
  }

  void AssertNull(const ExceptionOr<ConstPtr<ByteArray>>& bytes) {
    ASSERT_TRUE(bytes.ok());
    ASSERT_TRUE(bytes.result().isNull());
  }

  static constexpr int64_t kMaxSize = 3;

  std::string path_;
  std::fstream file_;
  size_t size_ = 0;
};

TEST_F(FileImplTest, InputFile_NonExistentPath) {
  InputFileImpl input_file("/not/a/valid/path.txt", GetSize());
  ExceptionOr<ConstPtr<ByteArray>> read_result = input_file.read(kMaxSize);
  ASSERT_FALSE(read_result.ok());
  ASSERT_EQ(read_result.exception(), Exception::IO);
}

TEST_F(FileImplTest, InputFile_GetFilePath) {
  InputFileImpl input_file(path_, GetSize());
  ASSERT_EQ(input_file.getFilePath(), path_);
}

TEST_F(FileImplTest, InputFile_EmptyFileEOF) {
  InputFileImpl input_file(path_, GetSize());
  AssertNull(input_file.read(kMaxSize));
}

TEST_F(FileImplTest, InputFile_ReadWorks) {
  WriteToFile("abc");
  InputFileImpl input_file(path_, GetSize());
  auto read_data = input_file.read(kMaxSize);
  read_data.result().destroy();
  SUCCEED();
}

TEST_F(FileImplTest, InputFile_ReadUntilEOF) {
  WriteToFile("abc");
  InputFileImpl input_file(path_, GetSize());
  AssertEquals(input_file.read(kMaxSize), "abc");
  AssertNull(input_file.read(kMaxSize));
}

TEST_F(FileImplTest, InputFile_ReadWithSize) {
  WriteToFile("abc");
  InputFileImpl input_file(path_, GetSize());
  AssertEquals(input_file.read(2), "ab");
  AssertEquals(input_file.read(1), "c");
  AssertNull(input_file.read(kMaxSize));
}

TEST_F(FileImplTest, InputFile_GetTotalSize) {
  WriteToFile("abc");
  InputFileImpl input_file(path_, GetSize());
  EXPECT_EQ(input_file.getTotalSize(), 3);
  AssertEquals(input_file.read(1), "a");
  EXPECT_EQ(input_file.getTotalSize(), 3);
}

TEST_F(FileImplTest, InputFile_Close) {
  WriteToFile("abc");
  InputFileImpl input_file(path_, GetSize());
  input_file.close();
  ExceptionOr<ConstPtr<ByteArray>> read_result = input_file.read(kMaxSize);
  ASSERT_FALSE(read_result.ok());
  ASSERT_EQ(read_result.exception(), Exception::IO);
}

TEST_F(FileImplTest, OutputFile_NonExistentPath) {
  OutputFileImpl output_file("/not/a/valid/path.txt");
  ConstPtr<ByteArray> bytes = MakeConstPtr(new ByteArray("a", 1));
  Exception::Value write_result = output_file.write(bytes);
  ASSERT_EQ(write_result, Exception::IO);
}

TEST_F(FileImplTest, OutputFile_Write) {
  OutputFileImpl output_file(path_);
  ConstPtr<ByteArray> bytes1 = MakeConstPtr(new ByteArray("a", 1));
  ConstPtr<ByteArray> bytes2 = MakeConstPtr(new ByteArray("bc", 2));
  ASSERT_EQ(output_file.write(bytes1), Exception::NONE);
  ASSERT_EQ(output_file.write(bytes2), Exception::NONE);
  InputFileImpl input_file(path_, GetSize());
  AssertEquals(input_file.read(kMaxSize), "abc");
}

TEST_F(FileImplTest, OutputFile_Close) {
  OutputFileImpl output_file(path_);
  output_file.close();
  ConstPtr<ByteArray> bytes = MakeConstPtr(new ByteArray("a", 1));
  ASSERT_EQ(output_file.write(bytes), Exception::IO);
}
}  // namespace nearby
}  // namespace location
