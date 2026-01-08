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

#include "internal/platform/file.h"

#include <cstddef>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"

namespace nearby {
namespace {

class FileTest : public ::testing::Test {
 protected:
  void SetUp() override { temp_dir_ = Files::GetTemporaryDirectory(); }

  FilePath GetTempFilePath(const std::string& file_name) {
    return FilePath(absl::StrCat(temp_dir_.ToString(), "/", file_name));
  }

  FilePath temp_dir_;
};

TEST_F(FileTest, ConstructorDestructorWorks) {
  // Setup
  FilePath file_path = GetTempFilePath("test_file.txt");
  std::string data = "test data";
  // Create an output file and write to it.
  OutputFile output_file(file_path.ToString());
  ASSERT_TRUE(output_file.IsValid());
  output_file.Write(data);
  output_file.Close();

  // Create an input file and read from it.
  InputFile input_file(file_path.ToString());
  ExceptionOr<ByteArray> read_bytes = input_file.Read(data.size());
  ASSERT_TRUE(read_bytes.ok());
  EXPECT_EQ(read_bytes.result(), ByteArray(data));
  input_file.Close();
}

TEST_F(FileTest, SimpleWriteRead) {
  FilePath file_path = GetTempFilePath("test_file.txt");
  std::string data = "ABCD";

  // Write to file.
  OutputFile output_file(file_path.ToString());
  ASSERT_TRUE(output_file.IsValid());
  EXPECT_TRUE(output_file.Write(data).Ok());
  EXPECT_TRUE(output_file.Close().Ok());

  // Read from file.
  InputFile input_file(file_path.ToString());
  ExceptionOr<ByteArray> read_data = input_file.Read(data.size());
  EXPECT_TRUE(read_data.ok());
  EXPECT_EQ(std::string(read_data.result()), data);
  EXPECT_TRUE(input_file.Close().Ok());
}

TEST_F(FileTest, WriteThenCloseThenRead) {
  FilePath file_path = GetTempFilePath("test_file_persistence.txt");
  std::string data = "Persistent data";

  // Write and close.
  OutputFile output_file(file_path.ToString());
  ASSERT_TRUE(output_file.IsValid());
  EXPECT_TRUE(output_file.Write(data).Ok());
  EXPECT_TRUE(output_file.Close().Ok());

  // Re-open and read.
  InputFile input_file(file_path.ToString());
  ExceptionOr<ByteArray> read_data = input_file.Read(data.size());
  EXPECT_TRUE(read_data.ok());
  EXPECT_EQ(std::string(read_data.result()), data);
  EXPECT_TRUE(input_file.Close().Ok());
}

TEST_F(FileTest, ReadEmptyFile) {
  FilePath file_path = GetTempFilePath("empty_file.txt");

  // Create empty file.
  OutputFile output_file(file_path.ToString());
  ASSERT_TRUE(output_file.IsValid());
  EXPECT_TRUE(output_file.Close().Ok());

  // Read from empty file.
  InputFile input_file(file_path.ToString());
  ExceptionOr<ByteArray> read_data = input_file.Read(1024);
  EXPECT_TRUE(read_data.ok());
  EXPECT_TRUE(read_data.result().Empty());
  EXPECT_TRUE(input_file.Close().Ok());
}

TEST_F(FileTest, ReadExactly) {
  FilePath file_path = GetTempFilePath("read_exactly_file.txt");
  std::string data = "read this exactly";

  OutputFile output_file(file_path.ToString());
  ASSERT_TRUE(output_file.IsValid());
  EXPECT_TRUE(output_file.Write(data).Ok());
  EXPECT_TRUE(output_file.Close().Ok());

  InputFile input_file(file_path.ToString());
  ExceptionOr<ByteArray> read_data =
      input_file.GetInputStream().ReadExactly(data.size());
  EXPECT_TRUE(read_data.ok());
  EXPECT_EQ(std::string(read_data.result()), data);
  EXPECT_TRUE(input_file.Close().Ok());
}

TEST_F(FileTest, ReadTooMuch) {
  FilePath file_path = GetTempFilePath("read_too_much_file.txt");
  std::string data = "some data";

  OutputFile output_file(file_path.ToString());
  ASSERT_TRUE(output_file.IsValid());
  EXPECT_TRUE(output_file.Write(data).Ok());
  EXPECT_TRUE(output_file.Close().Ok());

  InputFile input_file(file_path.ToString());
  ExceptionOr<ByteArray> read_data = input_file.Read(data.size() * 2);
  EXPECT_TRUE(read_data.ok());
  EXPECT_EQ(std::string(read_data.result()), data);
  EXPECT_TRUE(input_file.Close().Ok());
}

TEST_F(FileTest, Skip) {
  FilePath file_path = GetTempFilePath("skip_file.txt");
  std::string data_to_skip = "skip_this";
  std::string data_to_read = "read_this";
  std::string full_data = data_to_skip + data_to_read;

  OutputFile output_file(file_path.ToString());
  ASSERT_TRUE(output_file.IsValid());
  EXPECT_TRUE(output_file.Write(full_data).Ok());
  EXPECT_TRUE(output_file.Close().Ok());

  InputFile input_file(file_path.ToString());
  ExceptionOr<size_t> skipped_bytes = input_file.Skip(data_to_skip.size());
  EXPECT_TRUE(skipped_bytes.ok());
  EXPECT_EQ(skipped_bytes.result(), data_to_skip.size());

  ExceptionOr<ByteArray> read_data = input_file.Read(data_to_read.size());
  EXPECT_TRUE(read_data.ok());
  EXPECT_EQ(std::string(read_data.result()), data_to_read);
  EXPECT_TRUE(input_file.Close().Ok());
}

TEST_F(FileTest, MultipleWrites) {
  FilePath file_path = GetTempFilePath("multiple_writes.txt");
  std::string data1 = "first part, ";
  std::string data2 = "second part.";
  std::string full_data = data1 + data2;

  OutputFile output_file(file_path.ToString());
  ASSERT_TRUE(output_file.IsValid());
  EXPECT_TRUE(output_file.Write(data1).Ok());
  EXPECT_TRUE(output_file.Write(data2).Ok());
  EXPECT_TRUE(output_file.Close().Ok());

  InputFile input_file(file_path.ToString());
  ExceptionOr<ByteArray> read_data = input_file.Read(full_data.size());
  EXPECT_TRUE(read_data.ok());
  EXPECT_EQ(std::string(read_data.result()), full_data);
  EXPECT_TRUE(input_file.Close().Ok());
}

TEST_F(FileTest, CloseTwice) {
  FilePath file_path = GetTempFilePath("close_twice.txt");
  OutputFile output_file(file_path.ToString());
  ASSERT_TRUE(output_file.IsValid());
  EXPECT_TRUE(output_file.Close().Ok());
  EXPECT_TRUE(output_file.Close().Ok());

  InputFile input_file(file_path.ToString());
  EXPECT_TRUE(input_file.Close().Ok());
  EXPECT_TRUE(input_file.Close().Ok());
}

TEST_F(FileTest, WriteLargeFile) {
  FilePath file_path = GetTempFilePath("large_file.txt");
  std::string chunk(1024, 'a');
  std::string large_data;
  for (int i = 0; i < 10; ++i) {
    large_data += chunk;
  }

  OutputFile output_file(file_path.ToString());
  ASSERT_TRUE(output_file.IsValid());
  EXPECT_TRUE(output_file.Write(large_data).Ok());
  EXPECT_TRUE(output_file.Close().Ok());

  InputFile input_file(file_path.ToString());
  ExceptionOr<ByteArray> read_data =
      input_file.GetInputStream().ReadExactly(large_data.size());
  EXPECT_TRUE(read_data.ok());
  EXPECT_EQ(read_data.result().size(), large_data.size());
  EXPECT_EQ(std::string(read_data.result()), large_data);
  EXPECT_TRUE(input_file.Close().Ok());
}

}  // namespace
}  // namespace nearby
