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

#include "gtest/gtest.h"
#include "platform/api/platform.h"
#include "platform/base/exception.h"
#include "platform/base/payload_id.h"
#include "platform/impl/windows/test_utils.h"

class OutputFileTests : public testing::Test {
 protected:
  // You can define per-test set-up logic as usual.
  void SetUp() override {
    if (FileExists(TEST_FILE_PATH.c_str())) {
      DeleteFileA(TEST_FILE_PATH.c_str());
    }
  }

  // You can define per-test tear-down logic as usual.
  void TearDown() override {
    if (FileExists(TEST_FILE_PATH.c_str())) {
      DeleteFileA(TEST_FILE_PATH.c_str());
    }
  }

  BOOL FileExists(const char* szPath) {
    DWORD dwAttrib = GetFileAttributesA(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
            !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
  }
};

TEST_F(OutputFileTests, SuccessfulCreation) {
  std::unique_ptr<location::nearby::api::OutputFile> outputFile = nullptr;

  EXPECT_NO_THROW(
      outputFile =
          location::nearby::api::ImplementationPlatform::CreateOutputFile(
              TEST_FILE_PATH.c_str()));

  EXPECT_NE(outputFile, nullptr);
  EXPECT_NO_THROW(outputFile->Close());
}

TEST_F(OutputFileTests, SuccessfulClose) {
  std::unique_ptr<location::nearby::api::OutputFile> outputFile = nullptr;

  EXPECT_NO_THROW(
      outputFile =
          location::nearby::api::ImplementationPlatform::CreateOutputFile(
              TEST_FILE_PATH.c_str()));

  EXPECT_NO_THROW(outputFile->Close());

  DeleteFileA(TEST_FILE_PATH.c_str());
}

TEST_F(OutputFileTests, SuccessfulWrite) {
  location::nearby::ByteArray data(std::string(TEST_STRING));
  std::unique_ptr<location::nearby::api::OutputFile> outputFile = nullptr;

  EXPECT_NO_THROW(
      outputFile =
          location::nearby::api::ImplementationPlatform::CreateOutputFile(
              TEST_FILE_PATH.c_str()));

  EXPECT_NO_THROW(outputFile->Write(data));
  EXPECT_NO_THROW(outputFile->Close());

  DeleteFileA(TEST_FILE_PATH.c_str());
}
