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

#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/pipe.h"

namespace nearby {
namespace connections {
namespace {

using ::location::nearby::connections::PayloadTransferFrame;
constexpr char kText[] = "data chunk";

TEST(InternalPayloadFactoryTest, CanCreateInternalPayloadFromBytePayload) {
  ByteArray data(kText);
  std::unique_ptr<InternalPayload> internal_payload =
      CreateOutgoingInternalPayload(Payload{data});
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray(kText));
}

TEST(InternalPayloadFactoryTest, CanCreateInternalPayloadFromStreamPayload) {
  auto pipe = std::make_shared<Pipe>();
  std::unique_ptr<InternalPayload> internal_payload =
      CreateOutgoingInternalPayload(Payload{[pipe]() -> InputStream& {
        return pipe->GetInputStream();  // NOLINT
      }});
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_NE(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray());
}

TEST(InternalPayloadFactoryTest, CanCreateInternalPayloadFromFilePayload) {
  Payload::Id payload_id = Payload::GenerateId();
  InputFile inputFile(payload_id, 512);
  std::unique_ptr<InternalPayload> internal_payload =
      CreateOutgoingInternalPayload(Payload{payload_id, std::move(inputFile)});
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_NE(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray());
  EXPECT_EQ(payload.GetId(), payload_id);
}

TEST(InternalPayloadFactoryTest, CanCreateInternalPayloadFromByteMessage) {
  PayloadTransferFrame frame;
  std::string path = "C:\\Downloads";
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
  std::unique_ptr<InternalPayload> internal_payload =
      CreateIncomingInternalPayload(frame, path);
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray(kText));
}

TEST(InternalPayloadFactoryTest, CanCreateInternalPayloadFromStreamMessage) {
  PayloadTransferFrame frame;
  std::string path = "C:\\Downloads";
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::STREAM);
  header.set_id(12345);
  header.set_total_size(0);
  std::unique_ptr<InternalPayload> internal_payload =
      CreateIncomingInternalPayload(frame, path);
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
  std::string path = "C:\\Downloads";
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_id(12345);
  header.set_total_size(512);
  std::unique_ptr<InternalPayload> internal_payload =
      CreateIncomingInternalPayload(frame, path);
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
  std::string path = "C:\\Downloads";
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_total_size(512);
  std::unique_ptr<InternalPayload> internal_payload =
      CreateIncomingInternalPayload(frame, path);
  EXPECT_EQ(internal_payload, nullptr);
}

TEST(InternalPayloadFactoryTest,
     CanCreateInternalPayloadFromFileMessageWithFileNameNotSet) {
  PayloadTransferFrame frame;
  std::string path = "C:\\Downloads";
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_id(12345);
  header.set_total_size(512);
  std::unique_ptr<InternalPayload> internal_payload =
      CreateIncomingInternalPayload(frame, path);
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_EQ(payload.GetFileName(), "12345");
}
TEST(InternalPayloadFactoryTest,
     CanCreateInternalPayloadFromFileMessageWithFileNameSet) {
  PayloadTransferFrame frame;
  std::string path = "C:\\Downloads";
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_id(12345);
  header.set_total_size(512);
  header.set_file_name("test.file.name");
  std::unique_ptr<InternalPayload> internal_payload =
      CreateIncomingInternalPayload(frame, path);
  EXPECT_NE(internal_payload, nullptr);
  auto test = internal_payload->GetFileName();
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_EQ(payload.GetFileName(), "test.file.name");
}

void CreateFileWithContents(Payload::Id payload_id, const ByteArray& contents) {
  OutputFile file(payload_id);
  EXPECT_TRUE(file.Write(contents).Ok());
  EXPECT_TRUE(file.Close().Ok());
}

TEST(InternalPayloadFactoryTest,
     SkipToOffset_FilePayloadValidOffset_SkipsOffset) {
  ByteArray contents("0123456789");
  constexpr size_t kOffset = 4;
  size_t size_after_skip = contents.size() - kOffset;
  Payload::Id payload_id = Payload::GenerateId();
  CreateFileWithContents(payload_id, contents);
  InputFile inputFile(payload_id, contents.size());
  std::unique_ptr<InternalPayload> internal_payload =
      CreateOutgoingInternalPayload(Payload{payload_id, std::move(inputFile)});
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
     SkipToOffset_StreamPayloadValidOffset_SkipsOffset) {
  ByteArray contents("0123456789");
  constexpr size_t kOffset = 6;
  auto pipe = std::make_shared<Pipe>();
  std::unique_ptr<InternalPayload> internal_payload =
      CreateOutgoingInternalPayload(Payload{[pipe]() -> InputStream& {
        return pipe->GetInputStream();  // NOLINT
      }});
  EXPECT_NE(internal_payload, nullptr);
  pipe->GetOutputStream().Write(contents);

  ExceptionOr<size_t> result = internal_payload->SkipToOffset(kOffset);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.GetResult(), kOffset);
  EXPECT_EQ(internal_payload->GetTotalSize(), -1);
  ByteArray contents_after_skip = internal_payload->DetachNextChunk(512);
  EXPECT_EQ(contents_after_skip, ByteArray("6789"));
}

}  // namespace
}  // namespace connections
}  // namespace nearby
