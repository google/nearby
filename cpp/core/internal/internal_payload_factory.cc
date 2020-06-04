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

#include "core/payload.h"
#include "platform/api/condition_variable.h"
#include "platform/api/input_file.h"
#include "platform/api/lock.h"
#include "platform/api/output_file.h"
#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/pipe.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

class BytesInternalPayload : public InternalPayload {
 public:
  explicit BytesInternalPayload(ConstPtr<Payload> payload)
      : InternalPayload(payload),
        total_size_(payload_->asBytes()->size()),
        detached_only_chunk_(false) {}

  PayloadTransferFrame::PayloadHeader::PayloadType getType() const override {
    return PayloadTransferFrame::PayloadHeader::BYTES;
  }

  std::int64_t getTotalSize() const override { return total_size_; }

  ExceptionOr<ConstPtr<ByteArray> > detachNextChunk() override {
    if (detached_only_chunk_) {
      return ExceptionOr<ConstPtr<ByteArray> >(ConstPtr<ByteArray>());
    }

    detached_only_chunk_ = true;
    return ExceptionOr<ConstPtr<ByteArray> >(payload_->releaseBytes());
  }

  Exception::Value attachNextChunk(ConstPtr<ByteArray> chunk) override {
    // Avoid leaks.
    ScopedPtr<ConstPtr<ByteArray> > scoped_chunk(chunk);

    // Nothing to do - this method makes sense for other, more long-running
    // InternalPayload concrete implementations.
    return Exception::NONE;
  }

 private:
  // We're caching the total size here because the backing payload will be
  // released to another owner during the lifetime of an incoming
  // InternalPayload.
  const std::int64_t total_size_;
  bool detached_only_chunk_;
};

template <typename Platform>
class OutgoingStreamInternalPayload : public InternalPayload {
 public:
  explicit OutgoingStreamInternalPayload(ConstPtr<Payload> payload)
      : InternalPayload(payload) {}

  PayloadTransferFrame::PayloadHeader::PayloadType getType() const override {
    return PayloadTransferFrame::PayloadHeader::STREAM;
  }

  std::int64_t getTotalSize() const override { return -1; }

  ExceptionOr<ConstPtr<ByteArray> > detachNextChunk() override {
    Ptr<InputStream> input_stream(payload_->asStream()->asInputStream());

    ExceptionOr<ConstPtr<ByteArray> > bytes_read =
        input_stream->read(kChunkSize);
    if (!bytes_read.ok()) {
      if (Exception::IO == bytes_read.exception()) {
        // Ignore the potential Exception returned by close(), as a counterpart
        // to Java's closeQuietly().
        input_stream->close();
        return bytes_read;
      }
    }

    // Avoid leaks.
    ScopedPtr<ConstPtr<ByteArray> > scoped_bytes_read(bytes_read.result());

    if (scoped_bytes_read.isNull()) {
      // TODO(reznor): logger.atVerbose().log("No more data for outgoing payload
      // %s, closing InputStream.", this);

      // Ignore the potential Exception returned by close(), as a counterpart
      // to Java's closeQuietly().
      input_stream->close();
      return ExceptionOr<ConstPtr<ByteArray> >(ConstPtr<ByteArray>());
    }

    return ExceptionOr<ConstPtr<ByteArray> >(scoped_bytes_read.release());
  }

  Exception::Value attachNextChunk(ConstPtr<ByteArray> chunk) override {
    return Exception::IO;
  }

  void close() override {
    // Ignore the potential Exception returned by close(), as a counterpart
    // to Java's closeQuietly().
    payload_->asStream()->asInputStream()->close();
  }

 private:
  static constexpr std::int64_t kChunkSize = 64 * 1024;
};

template <typename Platform>
class IncomingStreamInternalPayload : public InternalPayload {
 public:
  IncomingStreamInternalPayload(ConstPtr<Payload> payload,
                                Ptr<OutputStream> output_stream)
      : InternalPayload(payload), output_stream_(output_stream) {}

  PayloadTransferFrame::PayloadHeader::PayloadType getType() const override {
    return PayloadTransferFrame::PayloadHeader::STREAM;
  }

  std::int64_t getTotalSize() const override { return -1; }

  ExceptionOr<ConstPtr<ByteArray> > detachNextChunk() override {
    return ExceptionOr<ConstPtr<ByteArray> >(Exception::IO);
  }

  Exception::Value attachNextChunk(ConstPtr<ByteArray> chunk) override {
    ScopedPtr<ConstPtr<ByteArray> > scoped_chunk(chunk);

    if (scoped_chunk.isNull()) {
      output_stream_->close();
      return Exception::NONE;
    }

    return output_stream_->write(scoped_chunk.release());
  }

  void close() override {
    output_stream_->close();
  }

 private:
  ScopedPtr<Ptr<OutputStream> > output_stream_;
};

class OutgoingFileInternalPayload : public InternalPayload {
 public:
  explicit OutgoingFileInternalPayload(ConstPtr<Payload> payload)
      : InternalPayload(std::move(payload)) {}

  PayloadTransferFrame::PayloadHeader::PayloadType getType() const override {
    return PayloadTransferFrame::PayloadHeader::FILE;
  }

  std::int64_t getTotalSize() const override {
    return payload_->asFile()->asInputFile()->getTotalSize();
  }

  ExceptionOr<ConstPtr<ByteArray>> detachNextChunk() override {
    Ptr<InputFile> input_file(payload_->asFile()->asInputFile());

    ExceptionOr<ConstPtr<ByteArray>> bytes_read = input_file->read(kChunkSize);
    if (!bytes_read.ok()) {
      if (Exception::IO == bytes_read.exception()) {
        input_file->close();
        return bytes_read;
      }
    }

    // Avoid leaks.
    ScopedPtr<ConstPtr<ByteArray>> scoped_bytes_read(bytes_read.result());

    if (scoped_bytes_read.isNull()) {
      // No more data for outgoing payload.

      input_file->close();
      return ExceptionOr<ConstPtr<ByteArray>>(ConstPtr<ByteArray>());
    }

    return ExceptionOr<ConstPtr<ByteArray>>(scoped_bytes_read.release());
  }

  Exception::Value attachNextChunk(ConstPtr<ByteArray> chunk) override {
    return Exception::IO;
  }

  void close() override { payload_->asFile()->asInputFile()->close(); }

 private:
  static constexpr std::int64_t kChunkSize = 64 * 1024;
};

class IncomingFileInternalPayload : public InternalPayload {
 public:
  IncomingFileInternalPayload(ConstPtr<Payload> payload,
                              const Ptr<OutputFile>& output_file,
                              std::int64_t total_size)
      : InternalPayload(std::move(payload)),
        output_file_(output_file),
        total_size_(total_size) {}

  PayloadTransferFrame::PayloadHeader::PayloadType getType() const override {
    return PayloadTransferFrame::PayloadHeader::FILE;
  }

  std::int64_t getTotalSize() const override { return total_size_; }

  ExceptionOr<ConstPtr<ByteArray>> detachNextChunk() override {
    return ExceptionOr<ConstPtr<ByteArray>>(Exception::IO);
  }

  Exception::Value attachNextChunk(ConstPtr<ByteArray> chunk) override {
    ScopedPtr<ConstPtr<ByteArray>> scoped_chunk(chunk);

    if (scoped_chunk.isNull()) {
      // Received null last chunk for incoming payload.
      output_file_->close();
      return Exception::NONE;
    }

    return output_file_->write(scoped_chunk.release());
  }

  void close() override { output_file_->close(); }

 private:
  ScopedPtr<Ptr<OutputFile>> output_file_;
  const std::int64_t total_size_;
};

}  // namespace

template <typename Platform>
Ptr<InternalPayload> InternalPayloadFactory<Platform>::createOutgoing(
    ConstPtr<Payload> payload) {
  // Avoid leaks.
  ScopedPtr<ConstPtr<Payload> > scoped_payload(payload);

  switch (scoped_payload->getType()) {
    case Payload::Type::BYTES:
      return MakePtr(new BytesInternalPayload(scoped_payload.release()));

    case Payload::Type::FILE:
      return MakePtr(new OutgoingFileInternalPayload(scoped_payload.release()));

    case Payload::Type::STREAM:
      return MakePtr(new OutgoingStreamInternalPayload<Platform>(
          scoped_payload.release()));

    default: {}
      // Fall through
  }

  // This should never be reached since the ServiceControllerRouter has already
  // checked whether or not we can work with this Payload type.
  return Ptr<InternalPayload>();
}

template <typename Platform>
Ptr<InternalPayload> InternalPayloadFactory<Platform>::createIncoming(
    const PayloadTransferFrame& payload_transfer_frame) {
  if (PayloadTransferFrame::DATA != payload_transfer_frame.packet_type()) {
    return Ptr<InternalPayload>();
  }

  const int64_t payload_id = payload_transfer_frame.payload_header().id();
  switch (payload_transfer_frame.payload_header().type()) {
    case PayloadTransferFrame::PayloadHeader::BYTES: {
      const string& body = payload_transfer_frame.payload_chunk().body();
      return MakePtr(new BytesInternalPayload(MakeConstPtr(new Payload(
          payload_id, MakeConstPtr(new ByteArray(body.data(), body.size()))))));
    }

    case PayloadTransferFrame::PayloadHeader::STREAM: {
      // pipe will be auto-destroyed when it is no longer referenced.
      auto pipe = MakeRefCountedPtr(new Pipe());

      return MakePtr(new IncomingStreamInternalPayload<Platform>(
          MakeConstPtr(
              new Payload(payload_id, MakeConstPtr(new Payload::Stream(
                                          Pipe::createInputStream(pipe))))),
          Pipe::createOutputStream(pipe)));
    }

    case PayloadTransferFrame::PayloadHeader::FILE: {
      Ptr<OutputFile> output_file = Platform::createOutputFile(payload_id);
      Ptr<InputFile> input_file = Platform::createInputFile(
          payload_id, payload_transfer_frame.payload_header().total_size());
      ConstPtr<Payload> payload = MakeConstPtr(
          new Payload(payload_id, MakeConstPtr(new Payload::File(input_file))));
      return MakePtr(new IncomingFileInternalPayload(
          payload, output_file,
          payload_transfer_frame.payload_header().total_size()));
    }
    default: {}
      // Fall through.
  }

  // This should never be reached since the ServiceControllerRouter has
  // already checked whether or not we can work with this Payload type.
  return Ptr<InternalPayload>();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
