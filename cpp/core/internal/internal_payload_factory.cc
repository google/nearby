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

#include "core/internal/internal_payload_factory.h"

#include <cstdint>
#include <memory>

#include "absl/memory/memory.h"
#include "core/payload.h"
#include "platform/base/byte_array.h"
#include "platform/base/exception.h"
#include "platform/public/condition_variable.h"
#include "platform/public/file.h"
#include "platform/public/logging.h"
#include "platform/public/mutex.h"
#include "platform/public/pipe.h"

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
  ByteArray DetachNextChunk(int chunk_size) override {
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

  ExceptionOr<size_t> SkipToOffset(size_t offset) override {
    NEARBY_LOGS(WARNING) << "Bytes payload does not support offsets";
    return {Exception::kIo};
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

  ByteArray DetachNextChunk(int chunk_size) override {
    InputStream* input_stream = payload_.AsStream();
    if (!input_stream) return {};

    ExceptionOr<ByteArray> bytes_read = input_stream->Read(chunk_size);
    if (!bytes_read.ok()) {
      input_stream->Close();
      return {};
    }

    ByteArray scoped_bytes_read = std::move(bytes_read.result());

    if (scoped_bytes_read.Empty()) {
      NEARBY_LOGS(INFO) << "No more data for outgoing payload " << this
                        << ", closing InputStream.";

      input_stream->Close();
      return {};
    }

    return scoped_bytes_read;
  }

  Exception AttachNextChunk(const ByteArray& chunk) override {
    return {Exception::kIo};
  }

  ExceptionOr<size_t> SkipToOffset(size_t offset) override {
    InputStream* stream = payload_.AsStream();
    if (stream == nullptr) return {Exception::kIo};

    ExceptionOr<size_t> real_offset = stream->Skip(offset);
    if (real_offset.ok() && real_offset.GetResult() == offset) {
      return real_offset;
    }
    // Close the outgoing stream on any error
    stream->Close();
    if (!real_offset.ok()) {
      return real_offset;
    }
    NEARBY_LOGS(WARNING) << "Skip offset: " << real_offset.GetResult()
                         << ", expected offset: " << offset << " for payload "
                         << this;
    return {Exception::kIo};
  }

  void Close() override {
    // Ignore the potential Exception returned by close(), as a counterpart
    // to Java's closeQuietly().
    InputStream* stream = payload_.AsStream();
    if (stream) stream->Close();
  }
};

class IncomingStreamInternalPayload : public InternalPayload {
 public:
  IncomingStreamInternalPayload(Payload payload, OutputStream& output_stream)
      : InternalPayload(std::move(payload)), output_stream_(&output_stream) {}

  PayloadTransferFrame::PayloadHeader::PayloadType GetType() const override {
    return PayloadTransferFrame::PayloadHeader::STREAM;
  }

  std::int64_t GetTotalSize() const override { return -1; }

  ByteArray DetachNextChunk(int chunk_size) override { return {}; }

  Exception AttachNextChunk(const ByteArray& chunk) override {
    if (chunk.Empty()) {
      NEARBY_LOGS(INFO) << "Received null last chunk for incoming payload "
                        << this << ", closing OutputStream.";
      output_stream_->Close();
      return {Exception::kSuccess};
    }

    return output_stream_->Write(chunk);
  }

  ExceptionOr<size_t> SkipToOffset(size_t offset) override {
    NEARBY_LOGS(WARNING) << "Cannot skip offset for an incoming Payload "
                         << this;
    return {Exception::kIo};
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

  ByteArray DetachNextChunk(int chunk_size) override {
    const InputFile* file = payload_.AsFile();
    if (!file) return {};

    ExceptionOr<ByteArray> bytes_read = file->Read(chunk_size);
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

  ExceptionOr<size_t> SkipToOffset(size_t offset) override {
    NEARBY_LOGS(INFO) << "SkipToOffset " << offset;
    const InputFile* file = payload_.AsFile();
    if (!file) {
      return {Exception::kIo};
    }

    ExceptionOr<size_t> real_offset = file->Skip(offset);
    if (real_offset.ok() && real_offset.GetResult() == offset) {
      return real_offset;
    }
    // Close the outgoing file on any error
    file->Close();
    if (!real_offset.ok()) {
      return real_offset;
    }
    NEARBY_LOGS(WARNING) << "Skip offset: " << real_offset.GetResult()
                         << ", expected offset: " << offset
                         << " for file payload " << this;
    return {Exception::kIo};
  }

  void Close() override {
    const InputFile* file = payload_.AsFile();
    if (file) file->Close();
  }

 private:
  std::int64_t total_size_;
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

  ByteArray DetachNextChunk(int chunk_size) override { return {}; }

  Exception AttachNextChunk(const ByteArray& chunk) override {
    if (chunk.Empty()) {
      // Received null last chunk for incoming payload.
      output_file_.Close();
      return {Exception::kSuccess};
    }

    return output_file_.Write(chunk);
  }

  ExceptionOr<size_t> SkipToOffset(size_t offset) override {
    NEARBY_LOGS(WARNING) << "Cannot skip offset for an incoming file Payload "
                         << this;
    return {Exception::kIo};
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

std::string make_path(std::string parent_folder, std::string file_name) {
  if (parent_folder.find_last_of('/') == std::string::npos) {
    parent_folder.append("/");
  }

  return parent_folder.append(file_name);
}

std::string make_path(std::string parent_folder, int64_t id) {
  if (parent_folder.find_last_of('/') == std::string::npos) {
    parent_folder.append("/");
  }

  return parent_folder.append(std::to_string(id));
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
      std::string file_path;
      int64_t total_size;

      if (frame.payload_header().has_parent_folder()) {
        file_path = frame.payload_header().parent_folder();
      }

      if (!frame.payload_header().has_file_name()) {
        file_path = make_path(file_path, frame.payload_header().id());
      } else {
        file_path = make_path(file_path, frame.payload_header().file_name());
      }

      if (frame.payload_header().has_total_size()) {
        total_size = frame.payload_header().total_size();
      }

      return absl::make_unique<IncomingFileInternalPayload>(
          Payload(payload_id, InputFile(file_path.c_str())),
          OutputFile(file_path.c_str()), frame.payload_header().total_size());
    }

    default:
      DCHECK(false);  // This should never happen.
      return {};
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
