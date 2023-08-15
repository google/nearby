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

#include "connections/payload.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/file.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/pipe.h"

namespace nearby {
namespace connections {

TEST(PayloadTest, DefaultPayloadHasUnknownType) {
  Payload payload;
  EXPECT_EQ(payload.GetType(), PayloadType::kUnknown);
}

TEST(PayloadTest, SupportsByteArrayType) {
  const ByteArray bytes("bytes");
  Payload payload(bytes);
  EXPECT_EQ(payload.GetType(), PayloadType::kBytes);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsBytes(), bytes);
}

TEST(PayloadTest, SupportsFileType) {
  constexpr size_t kOffset = 99;
  const auto payload_id = Payload::GenerateId();

  char test_file_data[100];
  memcpy(test_file_data,
         "012345678901234567890123456789012345678901234567890123456789012345678"
         "901234567890123456789012345678\0",
         100);

  OutputFile outputFile(payload_id);
  ByteArray test_data(test_file_data, 100);
  outputFile.Write(test_data);
  outputFile.Close();

  InputFile file(payload_id, 100);
  InputStream& stream = file.GetInputStream();

  Payload payload(payload_id, std::move(file));
  payload.SetOffset(kOffset);

  EXPECT_EQ(payload.GetFileName(), std::to_string(payload_id));
  EXPECT_EQ(payload.GetType(), PayloadType::kFile);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(&payload.AsFile()->GetInputStream(), &stream);
  EXPECT_EQ(payload.AsBytes(), ByteArray{});
  EXPECT_EQ(payload.GetOffset(), kOffset);
}

TEST(PayloadTest, SupportsMultiDotNamedFileType) {
  constexpr char expected[] = "this.is.a.multidot.file";
  InputFile file(expected, 0);

  Payload payload(std::move(file));

  EXPECT_EQ(payload.GetFileName(), expected);
}

TEST(PayloadTest,
     SupportsBackSlashFolderSeparatorsByExtractingFileNameBeforeStoring) {
  constexpr char file_name[] =
      "test_folder.here\\this.is.a.multidot.backslash.folder.separated.file";
  constexpr char expected[] =
      "this.is.a.multidot.backslash.folder.separated.file";
  InputFile file(file_name, 0);

  Payload payload(std::move(file));

  EXPECT_EQ(payload.GetFileName(), expected);
}

TEST(PayloadTest, SupportsStreamType) {
  constexpr size_t kOffset = 1234456;
  auto [input, output] = CreatePipe();
  InputStream* input_stream = input.get();

  Payload payload(std::move(input));
  payload.SetOffset(kOffset);

  EXPECT_EQ(payload.GetType(), PayloadType::kStream);
  EXPECT_EQ(payload.AsStream(), input_stream);
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray{});
  EXPECT_EQ(payload.GetOffset(), kOffset);
}

TEST(PayloadTest, PayloadIsMoveable) {
  Payload payload1;
  Payload payload2(ByteArray("bytes"));
  auto id = payload2.GetId();
  ByteArray bytes = payload2.AsBytes();
  EXPECT_EQ(payload1.GetType(), PayloadType::kUnknown);
  EXPECT_EQ(payload2.GetType(), PayloadType::kBytes);
  payload1 = std::move(payload2);
  EXPECT_EQ(payload1.GetType(), PayloadType::kBytes);
  EXPECT_EQ(payload1.AsBytes(), bytes);
  EXPECT_EQ(payload1.GetId(), id);
}

TEST(PayloadTest, PayloadHasUniqueId) {
  Payload payload1;
  Payload payload2;
  EXPECT_NE(payload1.GetId(), payload2.GetId());
}

TEST(PayloadTest, PayloadIsNotCopyable) {
  EXPECT_FALSE(std::is_copy_constructible_v<Payload>);
  EXPECT_FALSE(std::is_copy_assignable_v<Payload>);
}

}  // namespace connections
}  // namespace nearby
