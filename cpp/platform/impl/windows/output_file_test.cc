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

#include "platform/impl/windows/output_file.h"

#include "gtest/gtest.h"
#include "platform/api/platform.h"
#include "platform/base/exception.h"
#include "platform/base/payload_id.h"
#include "platform/impl/windows/test_utils.h"

class OutputFileTests : public testing::Test {
 protected:
  // You can define per-test set-up logic as usual.
  void SetUp() override {
    location::nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
    if (FileExists(test_utils::GetPayloadPath(payloadId).c_str())) {
      DeleteFileA(test_utils::GetPayloadPath(payloadId).c_str());
    }
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
};

TEST_F(OutputFileTests, SuccessfulCreation) {
  location::nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
  std::unique_ptr<location::nearby::api::OutputFile> outputFile = nullptr;

  EXPECT_NO_THROW(
      outputFile =
          location::nearby::api::ImplementationPlatform::CreateOutputFile(
              payloadId));

  EXPECT_NE(outputFile, nullptr);
  EXPECT_NO_THROW(outputFile->Close());
}

TEST_F(OutputFileTests, SuccessfulClose) {
  location::nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
  std::unique_ptr<location::nearby::api::OutputFile> outputFile = nullptr;

  EXPECT_NO_THROW(
      outputFile =
          location::nearby::api::ImplementationPlatform::CreateOutputFile(
              payloadId));

  EXPECT_NO_THROW(outputFile->Close());

  DeleteFileA(test_utils::GetPayloadPath(payloadId).c_str());
}

TEST_F(OutputFileTests, SuccessfulWrite) {
  location::nearby::PayloadId payloadId(TEST_PAYLOAD_ID);
  location::nearby::ByteArray data(std::string(TEST_STRING));
  std::unique_ptr<location::nearby::api::OutputFile> outputFile = nullptr;

  EXPECT_NO_THROW(
      outputFile =
          location::nearby::api::ImplementationPlatform::CreateOutputFile(
              payloadId));

  EXPECT_NO_THROW(outputFile->Write(data));
  EXPECT_NO_THROW(outputFile->Close());

  DeleteFileA(test_utils::GetPayloadPath(payloadId).c_str());
}
