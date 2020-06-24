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

#include "core_v2/internal/internal_payload_factory.h"

#include "core_v2/internal/offline_frames.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/public/pipe.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

constexpr char kText[] = "data chunk";

TEST(InternalPayloadFActoryTest, CanCreateIternalPayloadFromBytePayload) {
  ByteArray data(kText);
  std::unique_ptr<InternalPayload> internal_payload =
      CreateOutgoingInternalPayload(Payload{data});
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray(kText));
}

TEST(InternalPayloadFActoryTest, CanCreateIternalPayloadFromStreamPayload) {
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

TEST(InternalPayloadFActoryTest, CanCreateIternalPayloadFromFilePayload) {
  Payload::Id payload_id = Payload::GenerateId();
  std::unique_ptr<InternalPayload> internal_payload =
      CreateOutgoingInternalPayload(
          Payload{payload_id, InputFile(payload_id, 512)});
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_NE(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray());
  EXPECT_EQ(payload.GetId(), payload_id);
  EXPECT_EQ(payload.AsFile()->GetPayloadId(), payload_id);
}

TEST(InternalPayloadFActoryTest, CanCreateIternalPayloadFromByteMessage) {
  PayloadTransferFrame frame;
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
      CreateIncomingInternalPayload(frame);
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray(kText));
}

TEST(InternalPayloadFActoryTest, CanCreateIternalPayloadFromStreamMessage) {
  PayloadTransferFrame frame;
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::STREAM);
  header.set_id(12345);
  header.set_total_size(0);
  std::unique_ptr<InternalPayload> internal_payload =
      CreateIncomingInternalPayload(frame);
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_NE(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray());
  EXPECT_EQ(payload.GetType(), Payload::Type::kStream);
}

TEST(InternalPayloadFActoryTest, CanCreateIternalPayloadFromFileMessage) {
  PayloadTransferFrame frame;
  frame.set_packet_type(PayloadTransferFrame::DATA);
  auto& header = *frame.mutable_payload_header();
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_id(12345);
  header.set_total_size(512);
  std::unique_ptr<InternalPayload> internal_payload =
      CreateIncomingInternalPayload(frame);
  EXPECT_NE(internal_payload, nullptr);
  Payload payload = internal_payload->ReleasePayload();
  EXPECT_NE(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray());
  EXPECT_EQ(payload.GetType(), Payload::Type::kFile);
  EXPECT_EQ(payload.GetId(), payload.AsFile()->GetPayloadId());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
