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

#include "internal/platform/implementation/linux/input_file.h"

#include <fstream>

#include "gtest/gtest.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/test_utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/payload_id.h"

class InputFileTests : public testing::Test {
 protected:
  // You can define per-test set-up logic as usual.
  void SetUp() override {
    nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
    auto path = test_utils::GetPayloadPath(payloadId);

    file_.open(path, std::ios::out);

    if (!file_) {
      NEARBY_LOG(
          ERROR, "Failed to create OutputFile with payloadId: %s and error: %d",
          test_utils::GetPayloadPath(payloadId).c_str(), std::strerror(errno));
    }

    const char* buffer = TEST_STRING;

    file_.write(buffer, std::strlen(buffer));

    file_.close();
  }

  // You can define per-test tear-down logic as usual.
  void TearDown() override {
    nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
    if (std::filesystem::exists(test_utils::GetPayloadPath(payloadId))) {
      std::filesystem::remove(test_utils::GetPayloadPath(payloadId));
    }
  }

 private:
  std::fstream file_;
};

TEST_F(InputFileTests, SuccessfulCreation) {
  nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
  std::unique_ptr<nearby::api::InputFile> inputFile = nullptr;

  inputFile = nearby::api::ImplementationPlatform::CreateInputFile(
      payloadId, strlen(TEST_STRING));

  EXPECT_NE(inputFile, nullptr);
  EXPECT_EQ(inputFile->Close(), nearby::Exception{nearby::Exception::kSuccess});
}

TEST_F(InputFileTests, SuccessfulGetFilePath) {
  nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
  std::unique_ptr<nearby::api::InputFile> inputFile = nullptr;
  std::string fileName;

  inputFile = nearby::api::ImplementationPlatform::CreateInputFile(
      payloadId, strlen(TEST_STRING));

  fileName = inputFile->GetFilePath();

  EXPECT_EQ(inputFile->Close(), nearby::Exception{nearby::Exception::kSuccess});

  EXPECT_EQ(fileName, test_utils::GetPayloadPath(payloadId).c_str());
}

TEST_F(InputFileTests, SuccessfulGetTotalSize) {
  nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
  std::unique_ptr<nearby::api::InputFile> inputFile = nullptr;
  int64_t size = -1;

  inputFile = nearby::api::ImplementationPlatform::CreateInputFile(
      payloadId, strlen(TEST_STRING));

  size = inputFile->GetTotalSize();

  EXPECT_EQ(inputFile->Close(), nearby::Exception{nearby::Exception::kSuccess});

  EXPECT_EQ(size, strlen(TEST_STRING));
}

TEST_F(InputFileTests, SuccessfulRead) {
  nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
  std::unique_ptr<nearby::api::InputFile> inputFile = nullptr;

  inputFile = nearby::api::ImplementationPlatform::CreateInputFile(
      payloadId, strlen(TEST_STRING));

  auto fileSize = inputFile->GetTotalSize();
  auto dataRead = inputFile->Read(fileSize);

  EXPECT_TRUE(dataRead.ok());
  EXPECT_EQ(inputFile->Close(), nearby::Exception{nearby::Exception::kSuccess});

  EXPECT_STREQ(std::string(dataRead.result()).c_str(), TEST_STRING);
}

TEST_F(InputFileTests, FailedRead) {
  nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
  std::unique_ptr<nearby::api::InputFile> inputFile = nullptr;

  inputFile = nearby::api::ImplementationPlatform::CreateInputFile(
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
