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

#include "platform/impl/shared/file.h"

#include <codecvt>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <ostream>

#include "file/util/temp_path.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "platform/base/byte_array.h"

#define TEST_FILE_NAME std::string("testfilename.txt")
#define TEST_INVALID_PARENT_FOLDER \
  std::string("fake/path/that/has/not/been/created/")

namespace location {
namespace nearby {
namespace shared {

class FileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_path_ = std::make_unique<TempPath>(TempPath::Local);
    path_ = temp_path_->path() + "/" + TEST_FILE_NAME;
    std::ofstream output_file(path_);
    file_ = std::fstream(path_, std::fstream::in | std::fstream::out);
  }

  void WriteToFile(absl::string_view text) {
    file_ << text;
    file_.flush();
    size_ += text.size();
  }

  size_t GetSize() const { return size_; }

  void AssertEquals(const ExceptionOr<ByteArray>& bytes,
                    const std::string& expected) {
    EXPECT_TRUE(bytes.ok());
    EXPECT_EQ(std::string(bytes.result()), expected);
  }

  void AssertEmpty(const ExceptionOr<ByteArray>& bytes) {
    EXPECT_TRUE(bytes.ok());
    EXPECT_TRUE(bytes.result().Empty());
  }

  static constexpr int64_t kMaxSize = 3;

  std::unique_ptr<TempPath> temp_path_;
  std::string path_;
  std::fstream file_;
  size_t size_ = 0;
};

TEST_F(FileTest, InputFile_NonExistentPath) {
  InputFile input_file((TEST_INVALID_PARENT_FOLDER + TEST_FILE_NAME).c_str());
  ExceptionOr<ByteArray> read_result = input_file.Read(kMaxSize);
  EXPECT_FALSE(read_result.ok());
  EXPECT_TRUE(read_result.GetException().Raised(Exception::kIo));
}

TEST_F(FileTest, InputFile_GetFilePath) {
  InputFile input_file(path_.c_str());
  EXPECT_EQ(input_file.GetFilePath(), path_);
}

TEST_F(FileTest, InputFile_EmptyFileEOF) {
  InputFile input_file(path_.c_str());
  AssertEmpty(input_file.Read(kMaxSize));
}

TEST_F(FileTest, InputFile_ReadWorks) {
  WriteToFile("abc");
  InputFile input_file(path_.c_str());
  input_file.Read(kMaxSize);
  SUCCEED();
}

TEST_F(FileTest, InputFile_ReadUntilEOF) {
  WriteToFile("abc");
  InputFile input_file(path_.c_str());
  AssertEquals(input_file.Read(kMaxSize), "abc");
  AssertEmpty(input_file.Read(kMaxSize));
}

TEST_F(FileTest, InputFile_ReadWithSize) {
  WriteToFile("abc");
  InputFile input_file(path_.c_str());
  AssertEquals(input_file.Read(2), "ab");
  AssertEquals(input_file.Read(1), "c");
  AssertEmpty(input_file.Read(kMaxSize));
}

TEST_F(FileTest, InputFile_GetTotalSize) {
  WriteToFile("abc");
  InputFile input_file(path_.c_str());
  EXPECT_EQ(input_file.GetTotalSize(), 3);
  AssertEquals(input_file.Read(1), "a");
  EXPECT_EQ(input_file.GetTotalSize(), 3);
}

TEST_F(FileTest, InputFile_Close) {
  WriteToFile("abc");
  InputFile input_file(path_.c_str());
  input_file.Close();
  ExceptionOr<ByteArray> read_result = input_file.Read(kMaxSize);
  EXPECT_FALSE(read_result.ok());
  EXPECT_TRUE(read_result.GetException().Raised(Exception::kIo));
}

TEST_F(FileTest, OutputFile_NonExistentPath) {
  OutputFile output_file((TEST_INVALID_PARENT_FOLDER + TEST_FILE_NAME).c_str());
  ByteArray bytes("a", 1);
  EXPECT_TRUE(output_file.Write(bytes).Raised(Exception::kIo));
}

TEST_F(FileTest, OutputFile_Write) {
  OutputFile output_file(path_.c_str());
  ByteArray bytes1("a");
  ByteArray bytes2("bc");
  EXPECT_EQ(output_file.Write(bytes1), Exception{Exception::kSuccess});
  EXPECT_EQ(output_file.Write(bytes2), Exception{Exception::kSuccess});
  InputFile input_file(path_.c_str());
  AssertEquals(input_file.Read(kMaxSize), "abc");
}

TEST_F(FileTest, OutputFile_Close) {
  OutputFile output_file(path_.c_str());
  output_file.Close();
  ByteArray bytes("a");
  EXPECT_EQ(output_file.Write(bytes), Exception{Exception::kIo});
}

}  // namespace shared
}  // namespace nearby
}  // namespace location
