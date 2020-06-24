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

#include <cstdint>
#include <memory>

#include "core_v2/payload.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/exception.h"
#include "platform_v2/public/condition_variable.h"
#include "platform_v2/public/file.h"
#include "platform_v2/public/mutex.h"
#include "platform_v2/public/pipe.h"
#include "absl/memory/memory.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

class BytesInternalPayload : public InternalPayload {
 public:
  explicit BytesInternalPayload(Payload payload)
      : InternalPayload(std::move(payload)),
        total_size_(payload_.AsBytes().size()),
        detached_only_chunk_(false) {}

  PayloadTransferFrame::PayloadHeader::PayloadType GetType() const override {
    return PayloadTransferFrame::PayloadHeader::BYTES;
  }

  std::int64_t GetTotalSize() const override { return total_size_; }

  // Relinquishes ownership of the payload_; retrieves and returns the stored
  // ByteArray.
  ByteArray DetachNextChunk() override {
    if (detached_only_chunk_) {
      return {};
    }

    detached_only_chunk_ = true;
    return std::move(payload_).AsBytes();
  }

  // Does nothing.
  Exception AttachNextChunk(const ByteArray& chunk) override {
    return {Exception::kSuccess};
  }

 private:
  // We're caching the total size here because the backing payload will be
  // moved to another owner during the lifetime of an incoming
  // InternalPayload.
  const std::int64_t total_size_;
  bool detached_only_chunk_;
};

class OutgoingStreamInternalPayload : public InternalPayload {
 public:
  explicit OutgoingStreamInternalPayload(Payload payload)
      : InternalPayload(std::move(payload)) {}

  PayloadTransferFrame::PayloadHeader::PayloadType GetType() const override {
    return PayloadTransferFrame::PayloadHeader::STREAM;
  }

  std::int64_t GetTotalSize() const override { return -1; }

  ByteArray DetachNextChunk() override {
    InputStream* input_stream = payload_.AsStream();
    if (!input_stream) return {};

    ExceptionOr<ByteArray> bytes_read = input_stream->Read(kChunkSize);
    if (!bytes_read.ok()) {
      input_stream->Close();
      return {};
    }

    ByteArray scoped_bytes_read = std::move(bytes_read.result());

    if (scoped_bytes_read.Empty()) {
      // TODO(reznor): logger.atVerbose().log("No more data for outgoing payload
      // %s, closing InputStream.", this);

      input_stream->Close();
      return {};
    }

    return scoped_bytes_read;
  }

  Exception AttachNextChunk(const ByteArray& chunk) override {
    return {Exception::kIo};
  }

  void Close() override {
    // Ignore the potential Exception returned by close(), as a counterpart
    // to Java's closeQuietly().
    InputStream* stream = payload_.AsStream();
    if (stream) stream->Close();
  }

 private:
  static constexpr std::int64_t kChunkSize = Pipe::kChunkSize;
};

class IncomingStreamInternalPayload : public InternalPayload {
 public:
  IncomingStreamInternalPayload(Payload payload, OutputStream& output_stream)
      : InternalPayload(std::move(payload)), output_stream_(&output_stream) {}

  PayloadTransferFrame::PayloadHeader::PayloadType GetType() const override {
    return PayloadTransferFrame::PayloadHeader::STREAM;
  }

  std::int64_t GetTotalSize() const override { return -1; }

  ByteArray DetachNextChunk() override { return {}; }

  Exception AttachNextChunk(const ByteArray& chunk) override {
    if (chunk.Empty()) {
      output_stream_->Close();
      return {Exception::kSuccess};
    }

    return output_stream_->Write(chunk);
  }

  void Close() override { output_stream_->Close(); }

 private:
  OutputStream* output_stream_;
};

class OutgoingFileInternalPayload : public InternalPayload {
 public:
  explicit OutgoingFileInternalPayload(Payload payload)
      : InternalPayload(std::move(payload)),
        total_size_{payload_.AsFile()->GetTotalSize()} {}

  PayloadTransferFrame::PayloadHeader::PayloadType GetType() const override {
    return PayloadTransferFrame::PayloadHeader::FILE;
  }

  std::int64_t GetTotalSize() const override { return total_size_; }

  ByteArray DetachNextChunk() override {
    InputFile* file = payload_.AsFile();
    if (!file) return {};

    ExceptionOr<ByteArray> bytes_read = file->Read(kChunkSize);
    if (!bytes_read.ok()) {
      return {};
    }

    ByteArray bytes = std::move(bytes_read.result());

    if (bytes.Empty()) {
      // No more data for outgoing payload.

      file->Close();
      return {};
    }

    return bytes;
  }

  Exception AttachNextChunk(const ByteArray& chunk) override {
    return {Exception::kIo};
  }

  void Close() override {
    InputFile* file = payload_.AsFile();
    if (file) file->Close();
  }

 private:
  std::int64_t total_size_;
  static constexpr std::int64_t kChunkSize = 64 * 1024;
};

class IncomingFileInternalPayload : public InternalPayload {
 public:
  IncomingFileInternalPayload(Payload payload, OutputFile output_file,
                              std::int64_t total_size)
      : InternalPayload(std::move(payload)),
        output_file_(std::move(output_file)),
        total_size_(total_size) {}

  PayloadTransferFrame::PayloadHeader::PayloadType GetType() const override {
    return PayloadTransferFrame::PayloadHeader::FILE;
  }

  std::int64_t GetTotalSize() const override { return total_size_; }

  ByteArray DetachNextChunk() override { return {}; }

  Exception AttachNextChunk(const ByteArray& chunk) override {
    if (chunk.Empty()) {
      // Received null last chunk for incoming payload.
      output_file_.Close();
      return {Exception::kSuccess};
    }

    return output_file_.Write(chunk);
  }

  void Close() override { output_file_.Close(); }

 private:
  OutputFile output_file_;
  const std::int64_t total_size_;
};

}  // namespace

std::unique_ptr<InternalPayload> CreateOutgoingInternalPayload(
    Payload payload) {
  switch (payload.GetType()) {
    case Payload::Type::kBytes:
      return absl::make_unique<BytesInternalPayload>(std::move(payload));

    case Payload::Type::kFile: {
      InputFile* file = payload.AsFile();
      const PayloadId file_payload_id = file ? file->GetPayloadId() : 0;
      const PayloadId payload_id = payload.GetId();
      CHECK(payload_id == file_payload_id);
      return absl::make_unique<OutgoingFileInternalPayload>(std::move(payload));
    }

    case Payload::Type::kStream:
      return absl::make_unique<OutgoingStreamInternalPayload>(
          std::move(payload));

    default:
      DCHECK(false);  // This should never happen.
      return {};
  }
}

std::unique_ptr<InternalPayload> CreateIncomingInternalPayload(
    const PayloadTransferFrame& frame) {
  if (frame.packet_type() != PayloadTransferFrame::DATA) {
    return {};
  }

  const Payload::Id payload_id = frame.payload_header().id();
  switch (frame.payload_header().type()) {
    case PayloadTransferFrame::PayloadHeader::BYTES: {
      return absl::make_unique<BytesInternalPayload>(
          Payload(payload_id, ByteArray(frame.payload_chunk().body())));
    }

    case PayloadTransferFrame::PayloadHeader::STREAM: {
      auto pipe = std::make_shared<Pipe>();

      return absl::make_unique<IncomingStreamInternalPayload>(
          Payload(payload_id,
                  [pipe]() -> InputStream& {
                    return pipe->GetInputStream();  // NOLINT
                  }),
          pipe->GetOutputStream());
    }

    case PayloadTransferFrame::PayloadHeader::FILE: {
      std::int64_t total_size = frame.payload_header().total_size();
      return absl::make_unique<IncomingFileInternalPayload>(
          Payload(payload_id, InputFile(payload_id, total_size)),
          OutputFile(payload_id), total_size);
    }
    default:
      DCHECK(false);  // This should never happen.
      return {};
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
