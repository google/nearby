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

#include "internal/platform/implementation/shared/file.h"

#include <cstring>
#include <fstream>
#include <memory>
#include <ostream>
#include <string>

#include "file/util/temp_path.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace location {
namespace nearby {
namespace shared {

namespace {
constexpr char kFileName[] = "/increment_file_test.txt";
constexpr char kOneIterationFileName[] = "/increment_file_test (1).txt";
constexpr char kSecondIterationFileName[] = "/increment_file_test (2).txt";
constexpr char kNoDotsFileName[] = "/incrementfiletesttxt";
constexpr char kOneIterationNoDotsFileName[] = "/incrementfiletesttxt (1)";
constexpr char kMultipleDotsFileName[] = "/increment.file.test.txt";
constexpr char kOneIterationMultipleDotsFileName[] =
    "/increment (1).file.test.txt";
}  // namespace

class FileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_path_ = std::make_unique<TempPath>(TempPath::Local);
    path_ = temp_path_->path() + "/file.txt";
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

TEST_F(FileTest, IOFile_NonExistentPathInput) {
  auto io_file =
      shared::IOFile::CreateInputFile("/not/a/valid/path.txt", GetSize());
  ExceptionOr<ByteArray> read_result = io_file->Read(kMaxSize);
  EXPECT_FALSE(read_result.ok());
  EXPECT_TRUE(read_result.GetException().Raised(Exception::kIo));
}

TEST_F(FileTest, IOFile_GetFilePath) {
  auto io_file = shared::IOFile::CreateInputFile(path_, GetSize());
  EXPECT_EQ(io_file->GetFilePath(), path_);
}

TEST_F(FileTest, IOFile_EmptyFileEOF) {
  auto io_file = shared::IOFile::CreateInputFile(path_, GetSize());
  AssertEmpty(io_file->Read(kMaxSize));
}

TEST_F(FileTest, IOFile_ReadWorks) {
  WriteToFile("abc");
  auto io_file = shared::IOFile::CreateInputFile(path_, GetSize());
  io_file->Read(kMaxSize);
  SUCCEED();
}

TEST_F(FileTest, IOFile_ReadUntilEOF) {
  WriteToFile("abc");
  auto io_file = shared::IOFile::CreateInputFile(path_, GetSize());
  AssertEquals(io_file->Read(kMaxSize), "abc");
  AssertEmpty(io_file->Read(kMaxSize));
}

TEST_F(FileTest, IOFile_ReadWithSize) {
  WriteToFile("abc");
  auto io_file = shared::IOFile::CreateInputFile(path_, GetSize());
  AssertEquals(io_file->Read(2), "ab");
  AssertEquals(io_file->Read(1), "c");
  AssertEmpty(io_file->Read(kMaxSize));
}

TEST_F(FileTest, IOFile_GetTotalSize) {
  WriteToFile("abc");
  auto io_file = shared::IOFile::CreateInputFile(path_, GetSize());
  EXPECT_EQ(io_file->GetTotalSize(), 3);
  AssertEquals(io_file->Read(1), "a");
  EXPECT_EQ(io_file->GetTotalSize(), 3);
}

TEST_F(FileTest, IOFile_CloseInput) {
  WriteToFile("abc");
  auto io_file = shared::IOFile::CreateInputFile(path_, GetSize());
  io_file->Close();
  ExceptionOr<ByteArray> read_result = io_file->Read(kMaxSize);
  EXPECT_FALSE(read_result.ok());
  EXPECT_TRUE(read_result.GetException().Raised(Exception::kIo));
}

TEST_F(FileTest, IOFile_NonExistentPathOutput) {
  auto io_file = shared::IOFile::CreateOutputFile("/not/a/valid/path.txt");
  ByteArray bytes("a", 1);
  EXPECT_TRUE(io_file->Write(bytes).Raised(Exception::kIo));
}

TEST_F(FileTest, IOFile_Write) {
  auto io_file_output = shared::IOFile::CreateOutputFile(path_);
  ByteArray bytes1("a");
  ByteArray bytes2("bc");
  EXPECT_EQ(io_file_output->Write(bytes1), Exception{Exception::kSuccess});
  EXPECT_EQ(io_file_output->Write(bytes2), Exception{Exception::kSuccess});
  auto io_file_input =
      shared::IOFile::CreateInputFile(io_file_output->GetFilePath(), GetSize());
  AssertEquals(io_file_input->Read(kMaxSize), "abc");
}

TEST_F(FileTest, IOFile_CloseOutput) {
  auto io_file = shared::IOFile::CreateOutputFile(path_);
  io_file->Close();
  ByteArray bytes("a");
  EXPECT_EQ(io_file->Write(bytes), Exception{Exception::kIo});
}

TEST_F(FileTest, IOFile_FileDoesntExistCreatesFileWithPassedName) {
  // Arrange
  std::ifstream inputFile;

  std::string incrementFileName(temp_path_->path() + kFileName);

  inputFile.open(incrementFileName, std::ifstream::binary | std::ifstream::in);

  EXPECT_FALSE(inputFile.rdstate() == std::ifstream::goodbit);

  // Act
  auto io_file = shared::IOFile::CreateOutputFile(incrementFileName);
  io_file->Close();

  // Assert
  inputFile.clear();
  inputFile.open(incrementFileName, std::ifstream::binary | std::ifstream::in);

  EXPECT_TRUE(inputFile.rdstate() == std::ifstream::goodbit);
  inputFile.close();

  // Remove the file and check that it is removed
  std::remove(incrementFileName.c_str());
  inputFile.clear();
  inputFile.open(incrementFileName, std::ifstream::binary | std::ifstream::in);

  EXPECT_FALSE(inputFile.rdstate() == std::ifstream::goodbit);
}

TEST_F(FileTest, IOFile_FileExistsCreatesFileWithIncrementedName) {
  // Arrange
  std::ifstream inputFile;

  std::string incrementFileName(temp_path_->path() + kFileName);

  inputFile.open(incrementFileName, std::ifstream::binary | std::ifstream::in);
  EXPECT_FALSE(inputFile.rdstate() == std::ifstream::goodbit);

  inputFile.clear();
  inputFile.open(incrementFileName, std::ifstream::binary | std::ifstream::in);
  EXPECT_FALSE(inputFile.rdstate() == std::ifstream::goodbit);

  // Act
  auto io_file1 = shared::IOFile::CreateOutputFile(incrementFileName);
  io_file1->Close();
  EXPECT_EQ(io_file1->GetFilePath(), temp_path_->path() + kFileName);

  auto io_file2 = shared::IOFile::CreateOutputFile(incrementFileName);
  io_file2->Close();
  EXPECT_EQ(io_file2->GetFilePath(),
            temp_path_->path() + kOneIterationFileName);

  // Assert
  // File 1
  inputFile.clear();
  inputFile.open(io_file1->GetFilePath(),
                 std::ifstream::binary | std::ifstream::in);

  EXPECT_TRUE(inputFile.rdstate() == std::ifstream::goodbit);
  inputFile.close();

  // File 2
  inputFile.clear();
  inputFile.open(io_file2->GetFilePath(),
                 std::ifstream::binary | std::ifstream::in);

  EXPECT_TRUE(inputFile.rdstate() == std::ifstream::goodbit);
  inputFile.close();

  // Remove the file and check that it is removed
  // File 1
  std::remove(io_file1->GetFilePath().c_str());
  inputFile.clear();
  inputFile.open(io_file1->GetFilePath(), std::ios::binary | std::ios::in);

  EXPECT_FALSE(inputFile.rdstate() == std::ifstream::goodbit);

  // File 2
  std::remove(io_file2->GetFilePath().c_str());
  inputFile.clear();
  inputFile.open(incrementFileName, std::ios::binary | std::ios::in);

  EXPECT_FALSE(inputFile.rdstate() == std::ifstream::goodbit);
}
TEST_F(FileTest, IOFile_MultipleFilesExistCreateNextIncrementedFileName) {
  std::ifstream inputFile;

  std::string incrementFileName(temp_path_->path() + kFileName);

  inputFile.open(incrementFileName, std::fstream::binary | std::fstream::in);
  EXPECT_FALSE(inputFile.rdstate() == std::fstream::goodbit);

  inputFile.clear();
  inputFile.open(incrementFileName, std::fstream::binary | std::fstream::in);
  EXPECT_FALSE(inputFile.rdstate() == std::fstream::goodbit);

  inputFile.clear();
  inputFile.open(incrementFileName, std::fstream::binary | std::fstream::in);
  EXPECT_FALSE(inputFile.rdstate() == std::fstream::goodbit);

  // Act
  auto io_file1 = shared::IOFile::CreateOutputFile(incrementFileName);
  io_file1->Close();
  EXPECT_EQ(io_file1->GetFilePath(), temp_path_->path() + kFileName);

  auto io_file2 = shared::IOFile::CreateOutputFile(incrementFileName);
  io_file2->Close();
  EXPECT_EQ(io_file2->GetFilePath(),
            temp_path_->path() + kOneIterationFileName);

  auto io_file3 = shared::IOFile::CreateOutputFile(incrementFileName);
  io_file3->Close();
  EXPECT_EQ(io_file3->GetFilePath(),
            temp_path_->path() + kSecondIterationFileName);

  // Assert
  // File 1
  inputFile.clear();
  inputFile.open(io_file1->GetFilePath(),
                 std::fstream::binary | std::fstream::in);

  EXPECT_TRUE(inputFile.rdstate() == std::ifstream::goodbit);
  inputFile.close();

  // File 2
  inputFile.clear();
  inputFile.open(io_file2->GetFilePath(),
                 std::fstream::binary | std::fstream::in);

  EXPECT_TRUE(inputFile.rdstate() == std::fstream::goodbit);
  inputFile.close();

  // File 3
  inputFile.clear();
  inputFile.open(io_file3->GetFilePath(),
                 std::fstream::binary | std::fstream::in);

  EXPECT_TRUE(inputFile.rdstate() == std::fstream::goodbit);
  inputFile.close();

  // Remove the file and check that it is removed
  // File 1
  std::remove(io_file1->GetFilePath().c_str());
  inputFile.clear();
  inputFile.open(io_file1->GetFilePath(),
                 std::fstream::binary | std::fstream::in);

  EXPECT_FALSE(inputFile.rdstate() == std::ifstream::goodbit);

  // File 2
  std::remove(io_file2->GetFilePath().c_str());
  inputFile.clear();
  inputFile.open(incrementFileName, std::fstream::binary | std::fstream::in);

  EXPECT_FALSE(inputFile.rdstate() == std::ifstream::goodbit);

  // File 3
  std::remove(io_file3->GetFilePath().c_str());
  inputFile.clear();
  inputFile.open(incrementFileName, std::fstream::binary | std::fstream::in);

  EXPECT_FALSE(inputFile.rdstate() == std::ifstream::goodbit);
}
TEST_F(FileTest, IOFile_FileNameContainsMultipleDots) {  // Arrange
  std::ifstream inputFile;

  std::string incrementFileName(temp_path_->path() + kMultipleDotsFileName);

  inputFile.open(incrementFileName, std::fstream::binary | std::fstream::in);
  EXPECT_FALSE(inputFile.rdstate() == std::fstream::goodbit);

  inputFile.clear();
  inputFile.open(incrementFileName, std::fstream::binary | std::fstream::in);
  EXPECT_FALSE(inputFile.rdstate() == std::fstream::goodbit);

  // Act
  auto io_file1 = shared::IOFile::CreateOutputFile(incrementFileName);
  io_file1->Close();
  EXPECT_EQ(io_file1->GetFilePath(),
            temp_path_->path() + kMultipleDotsFileName);

  auto io_file2 = shared::IOFile::CreateOutputFile(incrementFileName);
  io_file2->Close();
  EXPECT_EQ(io_file2->GetFilePath(),
            temp_path_->path() + kOneIterationMultipleDotsFileName);

  // Assert
  // File 1
  inputFile.clear();
  inputFile.open(io_file1->GetFilePath(),
                 std::fstream::binary | std::fstream::in);

  EXPECT_TRUE(inputFile.rdstate() == std::fstream::goodbit);
  inputFile.close();

  // File 2
  inputFile.clear();
  inputFile.open(io_file2->GetFilePath(),
                 std::fstream::binary | std::fstream::in);

  EXPECT_TRUE(inputFile.rdstate() == std::fstream::goodbit);
  inputFile.close();

  // Remove the file and check that it is removed
  // File 1
  std::remove(io_file1->GetFilePath().c_str());
  inputFile.clear();
  inputFile.open(io_file1->GetFilePath(),
                 std::fstream::binary | std::fstream::in);

  EXPECT_FALSE(inputFile.rdstate() == std::ifstream::goodbit);

  // File 2
  std::remove(io_file2->GetFilePath().c_str());
  inputFile.clear();
  inputFile.open(incrementFileName, std::fstream::binary | std::fstream::in);

  EXPECT_FALSE(inputFile.rdstate() == std::ifstream::goodbit);
}
TEST_F(FileTest, IOFile_FileNameContainsNoDots) {
  std::ifstream inputFile;

  std::string incrementFileName(temp_path_->path() + kNoDotsFileName);

  inputFile.open(incrementFileName, std::fstream::binary | std::fstream::in);
  EXPECT_FALSE(inputFile.rdstate() == std::fstream::goodbit);

  inputFile.clear();
  inputFile.open(incrementFileName, std::fstream::binary | std::fstream::in);
  EXPECT_FALSE(inputFile.rdstate() == std::fstream::goodbit);

  // Act
  auto io_file1 = shared::IOFile::CreateOutputFile(incrementFileName);
  io_file1->Close();
  EXPECT_EQ(io_file1->GetFilePath(), temp_path_->path() + kNoDotsFileName);

  auto io_file2 = shared::IOFile::CreateOutputFile(incrementFileName);
  io_file2->Close();
  EXPECT_EQ(io_file2->GetFilePath(),
            temp_path_->path() + kOneIterationNoDotsFileName);

  // Assert
  // File 1
  inputFile.clear();
  inputFile.open(io_file1->GetFilePath(),
                 std::fstream::binary | std::fstream::in);

  EXPECT_TRUE(inputFile.rdstate() == std::fstream::goodbit);
  inputFile.close();

  // File 2
  inputFile.clear();
  inputFile.open(io_file2->GetFilePath(),
                 std::fstream::binary | std::fstream::in);

  EXPECT_TRUE(inputFile.rdstate() == std::fstream::goodbit);
  inputFile.close();

  // Remove the file and check that it is removed
  // File 1
  std::remove(io_file1->GetFilePath().c_str());
  inputFile.clear();
  inputFile.open(io_file1->GetFilePath(),
                 std::fstream::binary | std::fstream::in);

  EXPECT_FALSE(inputFile.rdstate() == std::ifstream::goodbit);

  // File 2
  std::remove(io_file2->GetFilePath().c_str());
  inputFile.clear();
  inputFile.open(incrementFileName, std::fstream::binary | std::fstream::in);

  EXPECT_FALSE(inputFile.rdstate() == std::ifstream::goodbit);
}
}  // namespace shared
}  // namespace nearby
}  // namespace location
