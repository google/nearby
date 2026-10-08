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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/implementation/internal_payload.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/payload.h"
#include "connections/payload_type.h"
#include "internal/base/file_path_sanitize.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/expected.h"
#include "internal/platform/file.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/pipe.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::connections::PayloadTransferFrame;
using ::location::nearby::proto::connections::OperationResultCode;
using ::nearby::api::ImplementationPlatform;

// if custom_save_path is empty, default download path is used
std::string make_path(const std::string& custom_save_path,
                      const std::string& parent_folder,
                      const std::string& file_name) {
  return ImplementationPlatform::GetCustomSavePath(custom_save_path,
                                                   parent_folder, file_name);
}

class OutgoingBytesInternalPayload : public OutgoingInternalPayload {
 public:
  explicit OutgoingBytesInternalPayload(Payload payload)
      : OutgoingInternalPayload(std::move(payload)),
        total_size_(payload_.AsBytes().size()) {}

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

  ExceptionOr<size_t> SkipToOffset(size_t offset) override {
    LOG(WARNING) << "Bytes payload does not support offsets";
    return {Exception::kIo};
  }

 private:
  const std::int64_t total_size_;
  bool detached_only_chunk_ = false;
};

// Unlike STREAM and FILE payloads, which are transferred across multiple chunks
// and written incrementally via AttachNextChunk() after ReleasePayload() hands
// the read handle to the client, a BYTES payload is sent in its entirety in the
// initial DATA frame (offset == 0), followed by an empty LAST_CHUNK frame.
// Because Payload holds a ByteArray by value and is immediately moved to the
// client via ReleasePayload() upon receipt of the initial frame, `payload` is
// populated with the full data at construction time.
class IncomingBytesInternalPayload : public IncomingInternalPayload {
 public:
  explicit IncomingBytesInternalPayload(Payload payload)
      : IncomingInternalPayload(std::move(payload)),
        total_size_(payload_.AsBytes().size()) {}

  location::nearby::connections::PayloadTransferFrame::PayloadHeader::
      PayloadType
      GetType() const override {
    return location::nearby::connections::PayloadTransferFrame::PayloadHeader::
        BYTES;
  }

  std::int64_t GetTotalSize() const override { return total_size_; }

  // No-op. PayloadManager::ProcessDataPacket() calls AttachNextChunk() for
  // every incoming DATA frame (both the initial data frame and the trailing
  // empty LAST_CHUNK frame), but the full byte array was already placed into
  // `payload_` at construction time.
  Exception AttachNextChunk(absl::string_view chunk) override {
    return {Exception::kSuccess};
  }

 private:
  // We're caching the total size here because the backing payload will be
  // moved to another owner during the lifetime of an incoming
  // InternalPayload.
  const std::int64_t total_size_;
};

class OutgoingStreamInternalPayload : public OutgoingInternalPayload {
 public:
  explicit OutgoingStreamInternalPayload(Payload payload)
      : OutgoingInternalPayload(std::move(payload)) {}

  location::nearby::connections::PayloadTransferFrame::PayloadHeader::
      PayloadType
      GetType() const override {
    return location::nearby::connections::PayloadTransferFrame::PayloadHeader::
        STREAM;
  }

  std::int64_t GetTotalSize() const override {
    return InternalPayload::kIndeterminateSize;
  }

  ByteArray DetachNextChunk(int chunk_size) override {
    InputStream* input_stream = payload_.AsStream();
    if (!input_stream) return {};

    ExceptionOr<ByteArray> bytes_read = input_stream->Read(chunk_size);
    if (!bytes_read.ok()) {
      Close();
      return {};
    }

    ByteArray scoped_bytes_read = std::move(bytes_read.result());

    if (scoped_bytes_read.Empty()) {
      LOG(INFO) << "No more data for outgoing payload " << this
                << ", closing InputStream.";

      Close();
      return {};
    }

    return scoped_bytes_read;
  }

  ExceptionOr<size_t> SkipToOffset(size_t offset) override {
    InputStream* stream = payload_.AsStream();
    if (stream == nullptr) return {Exception::kIo};

    ExceptionOr<size_t> real_offset = stream->Skip(offset);
    if (real_offset.ok() && real_offset.GetResult() == offset) {
      return real_offset;
    }
    // Close the outgoing stream on any error
    Close();
    if (!real_offset.ok()) {
      return real_offset;
    }
    LOG(WARNING) << "Skip offset: " << real_offset.GetResult()
                 << ", expected offset: " << offset << " for payload " << this;
    return {Exception::kIo};
  }

  void Close() override {
    if (is_closed_.Set(true)) return;
    // Ignore the potential Exception returned by close(), as a counterpart
    // to Java's closeQuietly().
    InputStream* stream = payload_.AsStream();
    if (stream) stream->Close();
  }

 private:
  AtomicBoolean is_closed_{false};
};

class IncomingStreamInternalPayload : public IncomingInternalPayload {
 public:
  IncomingStreamInternalPayload(Payload payload,
                                std::unique_ptr<OutputStream> output)
      : IncomingInternalPayload(std::move(payload)),
        output_(std::move(output)) {}

  PayloadTransferFrame::PayloadHeader::PayloadType GetType() const override {
    return PayloadTransferFrame::PayloadHeader::STREAM;
  }

  std::int64_t GetTotalSize() const override {
    return InternalPayload::kIndeterminateSize;
  }

  Exception AttachNextChunk(absl::string_view chunk) override {
    if (chunk.empty()) {
      LOG(INFO) << "Received null last chunk for incoming payload " << this
                << ", closing OutputStream.";
      Close();
      return {Exception::kSuccess};
    }

    return output_->Write(chunk);
  }

  void Close() override {
    if (is_closed_.Set(true)) return;
    output_->Close();
  }

 private:
  AtomicBoolean is_closed_{false};
  std::unique_ptr<OutputStream> output_;
};

class OutgoingFileInternalPayload : public OutgoingInternalPayload {
 public:
  explicit OutgoingFileInternalPayload(Payload payload)
      : OutgoingInternalPayload(std::move(payload)),
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

      Close();
      return {};
    }

    return bytes;
  }

  ExceptionOr<size_t> SkipToOffset(size_t offset) override {
    LOG(INFO) << "SkipToOffset " << offset;
    InputFile* file = payload_.AsFile();
    if (!file) {
      return {Exception::kIo};
    }

    ExceptionOr<size_t> real_offset = file->Skip(offset);
    if (real_offset.ok() && real_offset.GetResult() == offset) {
      return real_offset;
    }
    // Close the outgoing file on any error
    Close();
    if (!real_offset.ok()) {
      return real_offset;
    }
    LOG(WARNING) << "Skip offset: " << real_offset.GetResult()
                 << ", expected offset: " << offset << " for file payload "
                 << this;
    return {Exception::kIo};
  }

  void Close() override {
    if (is_closed_.Set(true)) return;
    InputFile* file = payload_.AsFile();
    if (file) file->Close();
  }

 private:
  AtomicBoolean is_closed_{false};
  std::int64_t total_size_;
};

class IncomingFileInternalPayload : public IncomingInternalPayload {
 public:
  IncomingFileInternalPayload(Payload payload, OutputFile output_file,
                              absl::Time last_modified_time,
                              std::int64_t total_size)
      : IncomingInternalPayload(std::move(payload)),
        output_file_(std::move(output_file)),
        last_modified_time_(last_modified_time),
        total_size_(total_size) {}

  location::nearby::connections::PayloadTransferFrame::PayloadHeader::
      PayloadType
      GetType() const override {
    return location::nearby::connections::PayloadTransferFrame::PayloadHeader::
        FILE;
  }

  std::int64_t GetTotalSize() const override { return total_size_; }

  Exception AttachNextChunk(absl::string_view chunk) override {
    if (chunk.empty()) {
      // Received null last chunk for incoming payload.
      Close();
      return {Exception::kSuccess};
    }
    // Truncate 1 incoming chunk to the total size of the payload.
    // If we receive more chunks after the total size is reached, we will return
    // an error.
    // For file payloads, total size must be valid.
    if (bytes_written_ + chunk.size() > total_size_) {
      if (total_size_ <= bytes_written_) {
        Close();
        return {Exception::kIo};
      }
      // TODO(b/511806938): Add metrics to track if this ever happens.  If not,
      // remove this.
      chunk = chunk.substr(0, total_size_ - bytes_written_);
      bytes_written_ = total_size_;
    } else {
      bytes_written_ += chunk.size();
    }

    return output_file_.Write(chunk);
  }

  void Close() override {
    if (is_closed_.Set(true)) return;
    output_file_.SetLastModifiedTime(last_modified_time_);
    output_file_.Close();
  }

 private:
  AtomicBoolean is_closed_{false};
  OutputFile output_file_;
  const absl::Time last_modified_time_;
  const int64_t total_size_;
  // Bytes written to the output file.
  int64_t bytes_written_ = 0;
};

}  // namespace

ErrorOr<std::unique_ptr<OutgoingInternalPayload>> CreateOutgoingInternalPayload(
    Payload payload) {
  switch (payload.GetType()) {
    case PayloadType::kBytes:
      return {
          std::make_unique<OutgoingBytesInternalPayload>(std::move(payload))};

    case PayloadType::kFile: {
      return {
          std::make_unique<OutgoingFileInternalPayload>(std::move(payload))};
    }

    case PayloadType::kStream:
      return {
          std::make_unique<OutgoingStreamInternalPayload>(std::move(payload))};

    default:
      DCHECK(false);  // This should never happen.
      return {Error(OperationResultCode::DETAIL_UNKNOWN)};
  }
}

ErrorOr<std::unique_ptr<IncomingInternalPayload>> CreateIncomingInternalPayload(
    const location::nearby::connections::PayloadTransferFrame& frame,
    const std::string& custom_save_path) {
  if (frame.packet_type() !=
      location::nearby::connections::PayloadTransferFrame::DATA) {
    return {Error(
        OperationResultCode::NEARBY_GENERIC_INCOMING_PAYLOAD_NOT_DATA_TYPE)};
  }

  const Payload::Id payload_id = frame.payload_header().id();
  switch (frame.payload_header().type()) {
    case PayloadTransferFrame::PayloadHeader::BYTES: {
      return {std::make_unique<IncomingBytesInternalPayload>(
          Payload(payload_id, ByteArray(frame.payload_chunk().body())))};
    }

    case PayloadTransferFrame::PayloadHeader::STREAM: {
      auto [input, output] = CreatePipe();

      return {std::make_unique<IncomingStreamInternalPayload>(
          Payload(payload_id, std::move(input)), std::move(output))};
    }

    case PayloadTransferFrame::PayloadHeader::FILE: {
      std::string parent_folder;
      std::string file_name;
      std::string file_path;

      int64_t total_size = 0;

      if (frame.payload_header().has_parent_folder()) {
        parent_folder =
            SanitizeRelativeFilePath(frame.payload_header().parent_folder());
      }

      if (frame.payload_header().has_file_name()) {
        file_name = SanitizeFileName(frame.payload_header().file_name());
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
          LOG(ERROR) << "File name not found in incoming file Payload, "
                        "and the Id wasn't found.";
          return {Error(OperationResultCode::IO_FILE_OPENING_ERROR)};
        }
      }

      if (frame.payload_header().has_total_size()) {
        total_size = frame.payload_header().total_size();
      }
      absl::Time last_modified_time = absl::Now();
      if (frame.payload_header().has_last_modified_timestamp_millis()) {
        last_modified_time = absl::FromUnixMillis(
            frame.payload_header().last_modified_timestamp_millis());
        VLOG(1) << "Received last modified time: " << last_modified_time
                << " for file: " << file_name;
      }

      // These are ordered, the output file must be created first otherwise
      // there will be no input file to open.
#if defined(NEARBY_CHROMIUM)
      // On Chrome the file path should be empty, so use the payload id.
      OutputFile output_file(payload_id);
      if (!output_file.IsValid()) {
        LOG(ERROR) << "Output file payload ID is not valid: " << payload_id;
        return {Error(OperationResultCode::IO_FILE_OPENING_ERROR)};
      }
      return {std::make_unique<IncomingFileInternalPayload>(
          Payload(payload_id, InputFile(payload_id)),
          std::move(output_file), last_modified_time, total_size)};
#else  // defined(NEARBY_CHROMIUM)
      OutputFile output_file(file_path);
      if (!output_file.IsValid()) {
        LOG(ERROR) << "Output file payload path is not valid: " << file_path;
        return {Error(OperationResultCode::IO_FILE_OPENING_ERROR)};
      }
      return {std::make_unique<IncomingFileInternalPayload>(
          Payload(payload_id, parent_folder, file_name,
                  InputFile(file_path)),
          std::move(output_file), last_modified_time, total_size)};
#endif  // defined(NEARBY_CHROMIUM)
    }
    default:
      DCHECK(false);  // This should never happen.
      return {Error(OperationResultCode::DETAIL_UNKNOWN)};
  }
}

}  // namespace connections
}  // namespace nearby
