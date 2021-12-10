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

#include "core/payload.h"

#include <stdio.h>

#include <fstream>
#include <memory>
#include <type_traits>

#include "file/util/temp_path.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/byte_array.h"
#include "platform/base/input_stream.h"
#include "platform/public/file.h"
#include "platform/public/pipe.h"

#define TEST_FILE_PARENT_DIRECTORY std::string("")
#define TEST_FILE_NAME std::string("testfilename.txt")
#define TEST_FILE_PATH TEST_FILE_NAME

namespace location {
namespace nearby {
namespace connections {

class PayloadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_path_ = std::make_unique<TempPath>(TempPath::Local);
    path_ = temp_path_->path() + "/" + TEST_FILE_NAME;
    file_ = std::fstream(path_, std::fstream::out | std::fstream::trunc);
    file_ << "This is a test file with a minimum of 101 characters. This is "
             "used to verify the InputFile in the payload_test google test.";
    file_.close();
  }

  void TearDown() override { std::remove(path_.c_str()); }

  std::fstream file_;
  std::unique_ptr<TempPath> temp_path_;
  std::string path_;
};
TEST_F(PayloadTest, DefaultPayloadHasUnknownType) {
  Payload payload;
  EXPECT_EQ(payload.GetType(), Payload::Type::kUnknown);
}

TEST_F(PayloadTest, SupportsByteArrayType) {
  const ByteArray bytes("bytes");
  Payload payload(bytes);
  EXPECT_EQ(payload.GetType(), Payload::Type::kBytes);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsBytes(), bytes);
}

TEST_F(PayloadTest, SupportsFileType) {
  constexpr size_t kOffset = 99;
  InputFile file(path_.c_str());
  const InputStream& stream = file.GetInputStream();

  Payload payload(TEST_FILE_PARENT_DIRECTORY.c_str(), TEST_FILE_PATH.c_str(),
                  std::move(file));
  payload.SetOffset(kOffset);

  EXPECT_EQ(payload.GetType(), Payload::Type::kFile);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(&payload.AsFile()->GetInputStream(), &stream);
  EXPECT_EQ(payload.AsBytes(), ByteArray{});
  EXPECT_EQ(payload.GetOffset(), kOffset);
}

TEST_F(PayloadTest, SupportsStreamType) {
  constexpr size_t kOffset = 1234456;
  auto pipe = std::make_shared<Pipe>();

  Payload payload([streamable = pipe]() -> InputStream& {
    // For some reason, linter warns us that we return a dangling reference.
    // This is not true: we return a reference to internal variable of a
    // shared_ptr<Pipe> which remains valid while Payload is valid, since
    // shared_ptr<Pipe> is captured by value.
    return streamable->GetInputStream();  // NOLINT
  });
  payload.SetOffset(kOffset);

  EXPECT_EQ(payload.GetType(), Payload::Type::kStream);
  EXPECT_EQ(payload.AsStream(), &pipe->GetInputStream());
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray{});
  EXPECT_EQ(payload.GetOffset(), kOffset);
}

TEST_F(PayloadTest, PayloadIsMoveable) {
  Payload payload1;
  Payload payload2(ByteArray("bytes"));
  auto id = payload2.GetId();
  ByteArray bytes = payload2.AsBytes();
  EXPECT_EQ(payload1.GetType(), Payload::Type::kUnknown);
  EXPECT_EQ(payload2.GetType(), Payload::Type::kBytes);
  payload1 = std::move(payload2);
  EXPECT_EQ(payload1.GetType(), Payload::Type::kBytes);
  EXPECT_EQ(payload1.AsBytes(), bytes);
  EXPECT_EQ(payload1.GetId(), id);
}

TEST_F(PayloadTest, PayloadHasUniqueId) {
  Payload payload1;
  Payload payload2;
  EXPECT_NE(payload1.GetId(), payload2.GetId());
}

TEST_F(PayloadTest, PayloadIsNotCopyable) {
  EXPECT_FALSE(std::is_copy_constructible_v<Payload>);
  EXPECT_FALSE(std::is_copy_assignable_v<Payload>);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
