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

#include "connections/implementation/internal_payload_factory.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "connections/implementation/offline_frames_validator.h"
#include "connections/payload.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/file.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/shared/file.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/os_name.h"
#include "internal/platform/pipe.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::connections::PayloadTransferFrame;

class BytesInternalPayload : public InternalPayload {
 public:
  explicit BytesInternalPayload(Payload payload)
      : InternalPayload(std::move(payload)),
        total_size_(payload_.AsBytes().size()),
        detached_only_chunk_(false) {}

  location::nearby::connections::PayloadTransferFrame::PayloadHeader::
      PayloadType
      GetType() const override {
    return location::nearby::connections::PayloadTransferFrame::PayloadHeader::
        BYTES;
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

  location::nearby::connections::PayloadTransferFrame::PayloadHeader::
      PayloadType
      GetType() const override {
    return location::nearby::connections::PayloadTransferFrame::PayloadHeader::
        STREAM;
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
  IncomingStreamInternalPayload(Payload payload, std::shared_ptr<Pipe> pipe)
      : InternalPayload(std::move(payload)), pipe_(pipe) {}

  PayloadTransferFrame::PayloadHeader::PayloadType GetType() const override {
    return PayloadTransferFrame::PayloadHeader::STREAM;
  }

  std::int64_t GetTotalSize() const override { return -1; }

  ByteArray DetachNextChunk(int chunk_size) override { return {}; }

  Exception AttachNextChunk(const ByteArray& chunk) override {
    if (chunk.Empty()) {
      NEARBY_LOGS(INFO) << "Received null last chunk for incoming payload "
                        << this << ", closing OutputStream.";
      Close();
      return {Exception::kSuccess};
    }

    return pipe_->GetOutputStream().Write(chunk);
  }

  ExceptionOr<size_t> SkipToOffset(size_t offset) override {
    NEARBY_LOGS(WARNING) << "Cannot skip offset for an incoming Payload "
                         << this;
    return {Exception::kIo};
  }

  void Close() override { pipe_->GetOutputStream().Close(); }

 private:
  std::shared_ptr<Pipe> pipe_;
};

class OutgoingFileInternalPayload : public InternalPayload {
 public:
  explicit OutgoingFileInternalPayload(Payload payload)
      : InternalPayload(std::move(payload)),
        total_size_{payload_.AsFile()->GetTotalSize()} {}

  location::nearby::connections::PayloadTransferFrame::PayloadHeader::
      PayloadType
      GetType() const override {
    return location::nearby::connections::PayloadTransferFrame::PayloadHeader::
        FILE;
  }

  std::int64_t GetTotalSize() const override { return total_size_; }

  ByteArray DetachNextChunk(int chunk_size) override {
    InputFile* file = payload_.AsFile();
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
    InputFile* file = payload_.AsFile();
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
    InputFile* file = payload_.AsFile();
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

  location::nearby::connections::PayloadTransferFrame::PayloadHeader::
      PayloadType
      GetType() const override {
    return location::nearby::connections::PayloadTransferFrame::PayloadHeader::
        FILE;
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

using ::nearby::api::ImplementationPlatform;
using ::nearby::api::OSName;

std::unique_ptr<InternalPayload> CreateOutgoingInternalPayload(
    Payload payload) {
  switch (payload.GetType()) {
    case PayloadType::kBytes:
      return absl::make_unique<BytesInternalPayload>(std::move(payload));

    case PayloadType::kFile: {
      return absl::make_unique<OutgoingFileInternalPayload>(std::move(payload));
    }

    case PayloadType::kStream:
      return absl::make_unique<OutgoingStreamInternalPayload>(
          std::move(payload));

    default:
      DCHECK(false);  // This should never happen.
      return {};
  }
}

// if custom_save_path is empty, default download path is used
std::string make_path(const std::string& custom_save_path,
                      std::string& parent_folder, std::string& file_name) {
  if (!custom_save_path.empty()) {
    std::string path = absl::StrCat(custom_save_path, "/", parent_folder);
    return api::ImplementationPlatform::GetCustomSavePath(path, file_name);
  }
  return api::ImplementationPlatform::GetDownloadPath(parent_folder, file_name);
}

// if custom_save_path is empty, default download path is used
std::string make_path(const std::string& custom_save_path,
                      std::string& parent_folder, int64_t id) {
  std::string file_name(std::to_string(id));
  if (!custom_save_path.empty()) {
    std::string path = absl::StrCat(custom_save_path, "/", parent_folder);
    return api::ImplementationPlatform::GetCustomSavePath(path, file_name);
  }
  return api::ImplementationPlatform::GetDownloadPath(parent_folder, file_name);
}

std::unique_ptr<InternalPayload> CreateIncomingInternalPayload(
    const location::nearby::connections::PayloadTransferFrame& frame,
    const std::string& custom_save_path) {
  if (frame.packet_type() !=
      location::nearby::connections::PayloadTransferFrame::DATA) {
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
          pipe);
    }

    case PayloadTransferFrame::PayloadHeader::FILE: {
      std::string parent_folder;
      std::string file_name;
      std::string file_path;

      int64_t total_size = 0;

      if (frame.payload_header().has_parent_folder()) {
        parent_folder = frame.payload_header().parent_folder();
      }

      if (frame.payload_header().has_file_name()) {
        file_name = frame.payload_header().file_name();
        // if custom_save_path is empty, default download path is used
        file_path = make_path(custom_save_path, parent_folder, file_name);
      } else {
        if (frame.payload_header().has_id()) {
          file_name = std::to_string(frame.payload_header().id());
          // if custom_save_path is empty, default download path is used
          file_path = make_path(custom_save_path, parent_folder, file_name);
        } else {
          // This is an error condition, we don't have any way to generate a
          // file name for the output file.
          NEARBY_LOGS(ERROR) << "File name not found in incoming file Payload, "
                                "and the Id wasn't found.";
          return {};
        }
      }

      if (frame.payload_header().has_total_size()) {
        total_size = frame.payload_header().total_size();
      }

      // These are ordered, the output file must be created first otherwise
      // there will be no input file to open.
      // On Chrome the file path should be empty, so use the payload id.
      if (ImplementationPlatform::GetCurrentOS() == OSName::kChromeOS) {
        return absl::make_unique<IncomingFileInternalPayload>(
            Payload(payload_id, InputFile(payload_id, total_size)),
            OutputFile(payload_id), total_size);
      } else {
        return absl::make_unique<IncomingFileInternalPayload>(
            Payload(payload_id, parent_folder, file_name,
                    InputFile(file_path, total_size)),
            OutputFile(file_path), total_size);
      }
    }
    default:
      DCHECK(false);  // This should never happen.
      return {};
  }
}

}  // namespace connections
}  // namespace nearby
