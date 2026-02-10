// Copyright 2020-2022 Google LLC
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

#include "connections/implementation/internal_payload_factory.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/implementation/internal_payload.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/payload.h"
#include "connections/payload_type.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/expected.h"
#include "internal/platform/file.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/pipe.h"

namespace nearby {
namespace connections {
namespace {

using ::location::nearby::connections::PayloadTransferFrame;
constexpr char kText[] = "data chunk";

TEST(InternalPayloadFactoryTest, CanCreateInternalPayloadFromBytePayload) {
  ByteArray data(kText);
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateOutgoingInternalPayload(Payload{data});
  ASSERT_FALSE(result.has_error());
  std::unique_ptr<InternalPayload> internal_payload = std::move(result.value());
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray(kText));
}

TEST(InternalPayloadFactoryTest, CanCreateInternalPayloadFromStreamPayload) {
  auto [input, output] = CreatePipe();
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateOutgoingInternalPayload(Payload(std::move(input)));
  ASSERT_FALSE(result.has_error());
  std::unique_ptr<InternalPayload> internal_payload = std::move(result.value());
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_NE(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray());
}

TEST(InternalPayloadFactoryTest, CanCreateInternalPayloadFromFilePayload) {
  Payload::Id payload_id = Payload::GenerateId();
  InputFile inputFile(payload_id);
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateOutgoingInternalPayload(Payload{payload_id, std::move(inputFile)});
  ASSERT_FALSE(result.has_error());
  std::unique_ptr<InternalPayload> internal_payload = std::move(result.value());
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_NE(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray());
  EXPECT_EQ(payload.GetId(), payload_id);
}

TEST(InternalPayloadFactoryTest, CanCreateInternalPayloadFromByteMessage) {
  PayloadTransferFrame frame;
  std::string path = ::testing::TempDir();
  frame.set_packet_type(PayloadTransferFrame::DATA);
  std::int64_t payload_chunk_offset = 0;
  ByteArray data(kText);
  PayloadTransferFrame::PayloadChunk payload_chunk;
  payload_chunk.set_offset(payload_chunk_offset);
  payload_chunk.set_body(std::string(std::move(data)));
  payload_chunk.set_flags(0);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_id(12345);
  header.set_total_size(512);
  *frame.mutable_payload_chunk() = std::move(payload_chunk);
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateIncomingInternalPayload(frame, path);
  ASSERT_FALSE(result.has_error());
  std::unique_ptr<InternalPayload> internal_payload = std::move(result.value());
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray(kText));
}

TEST(InternalPayloadFactoryTest, CanCreateInternalPayloadFromStreamMessage) {
  PayloadTransferFrame frame;
  std::string path = ::testing::TempDir();
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::STREAM);
  header.set_id(12345);
  header.set_total_size(0);
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateIncomingInternalPayload(frame, path);
  ASSERT_FALSE(result.has_error());
  std::unique_ptr<InternalPayload> internal_payload = std::move(result.value());
  EXPECT_NE(internal_payload, nullptr);
  {
    Payload payload = internal_payload->ReleasePayload();
    EXPECT_EQ(payload.AsFile(), nullptr);
    EXPECT_NE(payload.AsStream(), nullptr);
    EXPECT_EQ(payload.AsBytes(), ByteArray());
    EXPECT_EQ(payload.GetType(), PayloadType::kStream);
  }
  // Verifies that we can close InternalPayload after releasing (and destroying)
  // the payload
  internal_payload->Close();
}

TEST(InternalPayloadFactoryTest, CanCreateInternalPayloadFromFileMessage) {
  PayloadTransferFrame frame;
  std::string path = ::testing::TempDir();
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_id(12345);
  header.set_total_size(512);
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateIncomingInternalPayload(frame, path);
  ASSERT_FALSE(result.has_error());
  std::unique_ptr<InternalPayload> internal_payload = std::move(result.value());
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_NE(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray());
  EXPECT_EQ(payload.GetType(), PayloadType::kFile);
}

TEST(InternalPayloadFactoryTest,
     InternalPayloadFromFileMessageWithoutIdReturnsNullptr) {
  PayloadTransferFrame frame;
  std::string path = ::testing::TempDir();
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_total_size(512);
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateIncomingInternalPayload(frame, path);
  EXPECT_TRUE(result.has_error());
}

TEST(InternalPayloadFactoryTest,
     CanCreateInternalPayloadFromFileMessageWithFileNameNotSet) {
  PayloadTransferFrame frame;
  std::string path = ::testing::TempDir();
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_id(12345);
  header.set_total_size(512);
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateIncomingInternalPayload(frame, path);
  ASSERT_FALSE(result.has_error());
  std::unique_ptr<InternalPayload> internal_payload = std::move(result.value());
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_EQ(payload.GetFileName(), "12345");
}

TEST(InternalPayloadFactoryTest,
     CanCreateInternalPayloadFromFileMessageWithFileNameSet) {
  PayloadTransferFrame frame;
  std::string path = ::testing::TempDir();
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_id(12345);
  header.set_total_size(512);
  header.set_file_name("test.file.name");
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateIncomingInternalPayload(frame, path);
  ASSERT_FALSE(result.has_error());
  std::unique_ptr<InternalPayload> internal_payload = std::move(result.value());
  EXPECT_NE(internal_payload, nullptr);
  auto test = internal_payload->GetFileName();
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_EQ(payload.GetFileName(), "test.file.name");
}

TEST(InternalPayloadFactoryTest,
     VerifyFilePayloadFileNameParentFolderAndLastModifiedTime) {
  PayloadTransferFrame frame;
  std::string path = ::testing::TempDir();
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_id(12345);
  header.set_total_size(512);
  header.set_file_name("test_file_name");
  header.set_parent_folder("test_parent_folder");
  int64_t time_millis = absl::ToUnixMillis(absl::Now());
  header.set_last_modified_timestamp_millis(time_millis);
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateIncomingInternalPayload(frame, path);
  ASSERT_FALSE(result.has_error());
  std::unique_ptr<InternalPayload> internal_payload = std::move(result.value());
  EXPECT_NE(internal_payload, nullptr);
  EXPECT_EQ(internal_payload->GetFileName(), "test_file_name");
  EXPECT_EQ(internal_payload->GetParentFolder(), "test_parent_folder");
  // Allow for a 1ms error in the timestamp. This is due to the time being
  // converted to a double for the proto and then back to a time.
  EXPECT_LE(
      std::abs(absl::ToUnixMillis(internal_payload->GetLastModifiedTime()) -
             time_millis),
      1);
}

TEST(InternalPayloadFactoryTest,
     CreateInternalPayloadFailsIfFileCannotBeCreated) {
  PayloadTransferFrame frame;
  // /dev/null is a special file, no sub directories can be created
  std::string path = "/dev/null";
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_id(12345);
  header.set_total_size(512);
  header.set_file_name("test.file.name");
  header.set_parent_folder("Downloads2");
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateIncomingInternalPayload(frame, path);
  ASSERT_TRUE(result.has_error());
}

void CreateFileWithContents(Payload::Id payload_id,
                            absl::string_view contents) {
  OutputFile file(payload_id);
  EXPECT_TRUE(file.Write(contents).Ok());
  EXPECT_TRUE(file.Close().Ok());
}

TEST(InternalPayloadFactoryTest,
     SkipToOffset_FilePayloadValidOffset_SkipsOffset) {
  absl::string_view contents("0123456789");
  constexpr size_t kOffset = 4;
  size_t size_after_skip = contents.size() - kOffset;
  Payload::Id payload_id = Payload::GenerateId();
  CreateFileWithContents(payload_id, contents);
  InputFile inputFile(payload_id);
  ErrorOr<std::unique_ptr<InternalPayload>> internal_payload_result =
      CreateOutgoingInternalPayload(Payload{payload_id, std::move(inputFile)});
  ASSERT_FALSE(internal_payload_result.has_error());
  std::unique_ptr<InternalPayload> internal_payload =
      std::move(internal_payload_result.value());
  EXPECT_NE(internal_payload, nullptr);

  ExceptionOr<size_t> result = internal_payload->SkipToOffset(kOffset);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.GetResult(), kOffset);
  EXPECT_EQ(internal_payload->GetTotalSize(), contents.size());
  ByteArray contents_after_skip =
      internal_payload->DetachNextChunk(size_after_skip);
  EXPECT_EQ(contents_after_skip, ByteArray("456789"));
}

TEST(InternalPayloadFactoryTest,
     SkipToOffsetForBytesPayloadFailsIfOffsetIsTooLarge) {
  ByteArray data(kText);
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateOutgoingInternalPayload(Payload{data});
  ASSERT_FALSE(result.has_error());
  std::unique_ptr<InternalPayload> internal_payload = std::move(result.value());
  ASSERT_NE(internal_payload, nullptr);
  EXPECT_EQ(internal_payload->SkipToOffset(1024).exception(), Exception::kIo);
}

TEST(InternalPayloadFactoryTest,
     AttachNextChunkForOutgoingStreamPayloadFails) {
  auto [input, output] = CreatePipe();
  ErrorOr<std::unique_ptr<InternalPayload>> internal_payload_result =
      CreateOutgoingInternalPayload(Payload(std::move(input)));
  ASSERT_FALSE(internal_payload_result.has_error());
  std::unique_ptr<InternalPayload> internal_payload =
      std::move(internal_payload_result.value());
  EXPECT_NE(internal_payload, nullptr);
  EXPECT_EQ(internal_payload->AttachNextChunk("data"),
            Exception{Exception::kIo});
}

TEST(InternalPayloadFactoryTest,
     SkipToOffset_StreamPayloadValidOffset_SkipsOffset) {
  absl::string_view contents("0123456789");
  constexpr size_t kOffset = 6;
  auto [input, output] = CreatePipe();
  ErrorOr<std::unique_ptr<InternalPayload>> internal_payload_result =
      CreateOutgoingInternalPayload(Payload(std::move(input)));
  ASSERT_FALSE(internal_payload_result.has_error());
  std::unique_ptr<InternalPayload> internal_payload =
      std::move(internal_payload_result.value());
  EXPECT_NE(internal_payload, nullptr);
  output->Write(contents);

  ExceptionOr<size_t> result = internal_payload->SkipToOffset(kOffset);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.GetResult(), kOffset);
  EXPECT_EQ(internal_payload->GetTotalSize(), -1);
  ByteArray contents_after_skip = internal_payload->DetachNextChunk(512);
  EXPECT_EQ(contents_after_skip, ByteArray("6789"));
}

TEST(InternalPayloadFactoryTest, IncomingFilePayloadBehavesCorrectly) {
  PayloadTransferFrame frame;
  std::string path = ::testing::TempDir();
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_id(12345);
  const int64_t total_size = 512;
  header.set_total_size(total_size);
  header.set_file_name("test_file_name");
  header.set_parent_folder("test_parent_folder");
  header.set_last_modified_timestamp_millis(1234567890);
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateIncomingInternalPayload(frame, path);
  ASSERT_FALSE(result.has_error());
  std::unique_ptr<InternalPayload> internal_payload = std::move(result.value());
  ASSERT_NE(internal_payload, nullptr);

  EXPECT_EQ(internal_payload->GetType(),
            PayloadTransferFrame::PayloadHeader::FILE);
  EXPECT_EQ(internal_payload->GetTotalSize(), total_size);
  EXPECT_TRUE(internal_payload->DetachNextChunk(1024).Empty());
  EXPECT_EQ(internal_payload->SkipToOffset(1024).exception(),
            Exception::kIo);

  // Attach a chunk.
  std::string chunk1 = "chunk1";
  ASSERT_TRUE(internal_payload->AttachNextChunk(chunk1).Ok());

  // Attach another chunk.
  std::string chunk2 = "chunk2";
  ASSERT_TRUE(internal_payload->AttachNextChunk(chunk2).Ok());

  // Close payload by attaching empty chunk.
  ASSERT_TRUE(internal_payload->AttachNextChunk("").Ok());

  // Verify file content.
  Payload payload = internal_payload->ReleasePayload();
  InputFile* input_file = payload.AsFile();
  ASSERT_NE(input_file, nullptr);
  std::string expected_content_str = chunk1 + chunk2;
  ByteArray expected_content(expected_content_str);
  ExceptionOr<ByteArray> file_content =
      input_file->Read(expected_content.size());
  input_file->Close();
  ASSERT_TRUE(file_content.ok());
  EXPECT_EQ(file_content.result(), expected_content);
}

TEST(InternalPayloadFactoryTest, IncomingStreamPayloadBehavesCorrectly) {
  PayloadTransferFrame frame;
  std::string path = ::testing::TempDir();
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::STREAM);
  header.set_id(12345);
  header.set_total_size(0);
  ErrorOr<std::unique_ptr<InternalPayload>> result =
      CreateIncomingInternalPayload(frame, path);
  ASSERT_FALSE(result.has_error());
  std::unique_ptr<InternalPayload> internal_payload = std::move(result.value());
  ASSERT_NE(internal_payload, nullptr);

  EXPECT_EQ(internal_payload->GetType(),
            PayloadTransferFrame::PayloadHeader::STREAM);
  EXPECT_EQ(internal_payload->GetTotalSize(), -1);
  EXPECT_TRUE(internal_payload->DetachNextChunk(1024).Empty());
  EXPECT_EQ(internal_payload->SkipToOffset(1024).exception(), Exception::kIo);

  // Attach a chunk.
  std::string chunk1 = "chunk1";
  ASSERT_TRUE(internal_payload->AttachNextChunk(chunk1).Ok());

  // Attach another chunk.
  std::string chunk2 = "chunk2";
  ASSERT_TRUE(internal_payload->AttachNextChunk(chunk2).Ok());

  // Close payload by attaching empty chunk.
  ASSERT_TRUE(internal_payload->AttachNextChunk("").Ok());

  Payload payload = internal_payload->ReleasePayload();
  InputStream* input_stream = payload.AsStream();
  ASSERT_NE(input_stream, nullptr);

  // Read from input stream to verify.
  std::string result_str;
  while (true) {
    ExceptionOr<ByteArray> chunk = input_stream->Read(1024);
    ASSERT_TRUE(chunk.ok());
    if (chunk.result().Empty()) break;
    result_str.append(chunk.result().data(), chunk.result().size());
  }
  ByteArray result_bytes(result_str);
  std::string expected_content_str = chunk1 + chunk2;
  ByteArray expected_content(expected_content_str);
  EXPECT_EQ(result_bytes, expected_content);
  input_stream->Close();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
