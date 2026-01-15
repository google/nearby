// Copyright 2024 Google LLC
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

#include "internal/platform/implementation/linux/file.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace linux {
namespace {

constexpr char kTestContent[] = "Hello, World!";
constexpr char kTestFileName[] = "/tmp/nearby_file_test.txt";

class FileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Clean up any existing test file
    std::filesystem::remove(kTestFileName);
  }

  void TearDown() override {
    // Clean up test file
    std::filesystem::remove(kTestFileName);
  }
};

TEST_F(FileTest, CreateInputFileSuccess) {
  // Create a test file
  std::ofstream out(kTestFileName);
  out << kTestContent;
  out.close();

  auto file = IOFile::CreateInputFile(kTestFileName, sizeof(kTestContent));
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->GetFilePath(), kTestFileName);
  EXPECT_GT(file->GetTotalSize(), 0);
  file->Close();
}

TEST_F(FileTest, CreateOutputFileSuccess) {
  auto file = IOFile::CreateOutputFile(kTestFileName);
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->GetFilePath(), kTestFileName);
  file->Close();
}

TEST_F(FileTest, ReadFileSuccess) {
  // Create a test file
  std::ofstream out(kTestFileName);
  out << kTestContent;
  out.close();

  auto file = IOFile::CreateInputFile(kTestFileName, sizeof(kTestContent));
  ASSERT_NE(file, nullptr);

  auto result = file->Read(sizeof(kTestContent) - 1);
  ASSERT_TRUE(result.ok());
  
  ByteArray data = result.result();
  EXPECT_EQ(data.size(), sizeof(kTestContent) - 1);
  EXPECT_EQ(std::string(data.data(), data.size()), kTestContent);
  
  file->Close();
}

TEST_F(FileTest, WriteFileSuccess) {
  auto file = IOFile::CreateOutputFile(kTestFileName);
  ASSERT_NE(file, nullptr);

  ByteArray data(kTestContent, sizeof(kTestContent) - 1);
  Exception result = file->Write(data);
  EXPECT_EQ(result.value, Exception::kSuccess);
  
  file->Close();

  // Verify the file was written
  std::ifstream in(kTestFileName);
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  EXPECT_EQ(content, kTestContent);
}

TEST_F(FileTest, CloseFileSuccess) {
  auto file = IOFile::CreateOutputFile(kTestFileName);
  ASSERT_NE(file, nullptr);

  Exception result = file->Close();
  EXPECT_EQ(result.value, Exception::kSuccess);
}

TEST_F(FileTest, FlushFileSuccess) {
  auto file = IOFile::CreateOutputFile(kTestFileName);
  ASSERT_NE(file, nullptr);

  ByteArray data(kTestContent, sizeof(kTestContent) - 1);
  file->Write(data);
  
  Exception result = file->Flush();
  EXPECT_EQ(result.value, Exception::kSuccess);
  
  file->Close();
}

}  // namespace
}  // namespace linux
}  // namespace nearby
