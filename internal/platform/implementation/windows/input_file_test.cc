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

#include "internal/platform/implementation/windows/input_file.h"

#include "gtest/gtest.h"
#include "internal/platform/exception.h"
#include "internal/platform/payload_id.h"
#include "internal/platform/implementation/windows/test_utils.h"
#include "internal/platform/logging.h"

class InputFileTests : public testing::Test {
 protected:
  // You can define per-test set-up logic as usual.
  void SetUp() override {
    location::nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
    auto path = test_utils::GetPayloadPath(payloadId);

    hFile_ = CreateFileA(path.c_str(),           // name of the write
                         GENERIC_WRITE,          // open for writing
                         0,                      // do not share
                         NULL,                   // default security
                         CREATE_ALWAYS,          // create new file only
                         FILE_ATTRIBUTE_NORMAL,  // normal file
                         NULL);                  // no attr. template

    if (hFile_ == INVALID_HANDLE_VALUE) {
      NEARBY_LOG(ERROR,
                 "Failed to create OutputFile with payloadId: %s and error: %d",
                 test_utils::GetPayloadPath(payloadId).c_str(), GetLastError());
    }

    const char* buffer = TEST_STRING;
    DWORD bytesWritten;

    WriteFile(hFile_, buffer, lstrlenA(buffer) * sizeof(char), &bytesWritten,
              nullptr);

    CloseHandle(hFile_);
  }

  // You can define per-test tear-down logic as usual.
  void TearDown() override {
    location::nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
    if (FileExists(test_utils::GetPayloadPath(payloadId).c_str())) {
      DeleteFileA(test_utils::GetPayloadPath(payloadId).c_str());
    }
  }

  BOOL FileExists(const char* szPath) {
    DWORD dwAttrib = GetFileAttributesA(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
            !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
  }

 private:
  HANDLE hFile_ = nullptr;
};

TEST_F(InputFileTests, SuccessfulCreation) {
  location::nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
  std::unique_ptr<location::nearby::api::InputFile> inputFile = nullptr;

  inputFile = location::nearby::api::ImplementationPlatform::CreateInputFile(
      payloadId, strlen(TEST_STRING));

  EXPECT_NE(inputFile, nullptr);
  EXPECT_EQ(inputFile->Close(),
            location::nearby::Exception{location::nearby::Exception::kSuccess});
}

TEST_F(InputFileTests, SuccessfulGetFilePath) {
  location::nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
  std::unique_ptr<location::nearby::api::InputFile> inputFile = nullptr;
  std::string fileName;

  inputFile = location::nearby::api::ImplementationPlatform::CreateInputFile(
      payloadId, strlen(TEST_STRING));

  fileName = inputFile->GetFilePath();

  EXPECT_EQ(inputFile->Close(),
            location::nearby::Exception{location::nearby::Exception::kSuccess});

  EXPECT_EQ(fileName, test_utils::GetPayloadPath(payloadId).c_str());
}

TEST_F(InputFileTests, SuccessfulGetTotalSize) {
  location::nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
  std::unique_ptr<location::nearby::api::InputFile> inputFile = nullptr;
  int64_t size = -1;

  inputFile = location::nearby::api::ImplementationPlatform::CreateInputFile(
      payloadId, strlen(TEST_STRING));

  size = inputFile->GetTotalSize();

  EXPECT_EQ(inputFile->Close(),
            location::nearby::Exception{location::nearby::Exception::kSuccess});

  EXPECT_EQ(size, strlen(TEST_STRING));
}

TEST_F(InputFileTests, SuccessfulRead) {
  location::nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
  std::unique_ptr<location::nearby::api::InputFile> inputFile = nullptr;

  inputFile = location::nearby::api::ImplementationPlatform::CreateInputFile(
      payloadId, strlen(TEST_STRING));

  auto fileSize = inputFile->GetTotalSize();
  auto dataRead = inputFile->Read(fileSize);

  EXPECT_TRUE(dataRead.ok());
  EXPECT_EQ(inputFile->Close(),
            location::nearby::Exception{location::nearby::Exception::kSuccess});

  EXPECT_STREQ(std::string(dataRead.result()).c_str(), TEST_STRING);
}

TEST_F(InputFileTests, FailedRead) {
  location::nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
  std::unique_ptr<location::nearby::api::InputFile> inputFile = nullptr;

  inputFile = location::nearby::api::ImplementationPlatform::CreateInputFile(
      payloadId, strlen(TEST_STRING));

  auto fileSize = inputFile->GetTotalSize();
  EXPECT_NE(fileSize, -1);

  auto dataRead = inputFile->Read(fileSize);
  EXPECT_TRUE(dataRead.ok());

  dataRead = inputFile->Read(fileSize);
  std::string data = std::string(dataRead.result());

  inputFile->Close();

  EXPECT_STREQ(data.c_str(), "");
}
