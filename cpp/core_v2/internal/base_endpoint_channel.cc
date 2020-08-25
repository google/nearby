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

#include "core_v2/internal/base_endpoint_channel.h"

#include <cassert>

#include "core_v2/internal/offline_frames.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/exception.h"
#include "platform_v2/public/logging.h"
#include "platform_v2/public/mutex.h"
#include "platform_v2/public/mutex_lock.h"
#include "proto/connections_enums.pb.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

std::int32_t BytesToInt(const ByteArray& bytes) {
  const char* int_bytes = bytes.data();

  std::int32_t result = 0;
  result |= (static_cast<std::int32_t>(int_bytes[0]) & 0x0FF) << 24;
  result |= (static_cast<std::int32_t>(int_bytes[1]) & 0x0FF) << 16;
  result |= (static_cast<std::int32_t>(int_bytes[2]) & 0x0FF) << 8;
  result |= (static_cast<std::int32_t>(int_bytes[3]) & 0x0FF);

  return result;
}

ByteArray IntToBytes(std::int32_t value) {
  char int_bytes[sizeof(std::int32_t)];
  int_bytes[0] = static_cast<char>((value >> 24) & 0x0FF);
  int_bytes[1] = static_cast<char>((value >> 16) & 0x0FF);
  int_bytes[2] = static_cast<char>((value >> 8) & 0x0FF);
  int_bytes[3] = static_cast<char>((value)&0x0FF);

  return ByteArray(int_bytes, sizeof(int_bytes));
}

ExceptionOr<ByteArray> ReadExactly(InputStream* reader, std::int64_t size) {
  ByteArray buffer(size);
  std::int64_t current_pos = 0;

  while (current_pos < size) {
    ExceptionOr<ByteArray> read_bytes = reader->Read(size - current_pos);
    if (!read_bytes.ok()) {
      return read_bytes;
    }
    ByteArray result = read_bytes.result();

    if (result.Empty()) {
      return ExceptionOr<ByteArray>(Exception::kIo);
    }

    buffer.CopyAt(current_pos, result);
    current_pos += result.size();
  }

  return ExceptionOr<ByteArray>(std::move(buffer));
}

ExceptionOr<std::int32_t> ReadInt(InputStream* reader) {
  ExceptionOr<ByteArray> read_bytes = ReadExactly(reader, sizeof(std::int32_t));
  if (!read_bytes.ok()) {
    return ExceptionOr<std::int32_t>(read_bytes.exception());
  }
  return ExceptionOr<std::int32_t>(BytesToInt(std::move(read_bytes.result())));
}

Exception WriteInt(OutputStream* writer, std::int32_t value) {
  return writer->Write(IntToBytes(value));
}

}  // namespace

BaseEndpointChannel::BaseEndpointChannel(const std::string& channel_name,
                                         InputStream* reader,
                                         OutputStream* writer)
    : channel_name_(channel_name), reader_(reader), writer_(writer) {}

ExceptionOr<ByteArray> BaseEndpointChannel::Read() {
  ByteArray result;
  {
    MutexLock lock(&reader_mutex_);

    ExceptionOr<std::int32_t> read_int = ReadInt(reader_);
    if (!read_int.ok()) {
      return ExceptionOr<ByteArray>(read_int.exception());
    }

    if (read_int.result() < 0 || read_int.result() > kMaxAllowedReadBytes) {
      return ExceptionOr<ByteArray>(Exception::kIo);
    }

    ExceptionOr<ByteArray> read_bytes = ReadExactly(reader_, read_int.result());
    if (!read_bytes.ok()) {
      return read_bytes;
    }
    result = std::move(read_bytes.result());
  }

  {
    MutexLock crypto_lock(&crypto_mutex_);
    if (IsEncryptionEnabledLocked()) {
      // If encryption is enabled, decode the message.
      std::string input(std::move(result));
      std::unique_ptr<std::string> decrypted_data =
          crypto_context_->DecodeMessageFromPeer(input);
      if (decrypted_data) {
        result = ByteArray(std::move(*decrypted_data));
      } else {
        // It could be a protocol race, where remote party sends a KEEP_ALIVE
        // before encryption is setup on their side, and we receive it after
        // we switched to encryption mode.
        // In this case, we verify that message is indeed a valid KEEP_ALIVE,
        // and let it through if it is, otherwise message is erased.
        // TODO(apolyudov): verify this happens at most once per session.
        result = {};
        auto parsed = parser::FromBytes(ByteArray(input));
        if (parsed.ok() &&
            parser::GetFrameType(parsed.result()) == V1Frame::KEEP_ALIVE) {
          result = ByteArray(input);
        }
      }
      if (result.Empty()) {
        return ExceptionOr<ByteArray>(Exception::kInvalidProtocolBuffer);
      }
    }
  }

  {
    MutexLock lock(&last_read_mutex_);
    last_read_timestamp_ = SystemClock::ElapsedRealtime();
  }
  return ExceptionOr<ByteArray>(result);
}

Exception BaseEndpointChannel::Write(const ByteArray& data) {
  {
    MutexLock pause_lock(&is_paused_mutex_);
    if (is_paused_) {
      BlockUntilUnpaused();
    }
  }

  ByteArray encrypted_data;
  const ByteArray* data_to_write = &data;
  {
    MutexLock crypto_lock(&crypto_mutex_);
    if (IsEncryptionEnabledLocked()) {
      // If encryption is enabled, encode the message.
      std::unique_ptr<std::string> encrypted =
          crypto_context_->EncodeMessageToPeer(std::string(data));
      if (!encrypted) return {Exception::kIo};
      encrypted_data = ByteArray(std::move(*encrypted));
      data_to_write = &encrypted_data;
    }
  }

  {
    MutexLock lock(&writer_mutex_);
    Exception write_exception =
        WriteInt(writer_, static_cast<std::int32_t>(data_to_write->size()));
    if (write_exception.Raised()) {
      return write_exception;
    }
    write_exception = writer_->Write(*data_to_write);
    if (write_exception.Raised()) {
      return write_exception;
    }
    Exception flush_exception = writer_->Flush();
    if (flush_exception.Raised()) {
      return flush_exception;
    }
  }

  return {Exception::kSuccess};
}

void BaseEndpointChannel::Close() {
  {
    // In case channel is paused, resume it first thing.
    MutexLock lock(&is_paused_mutex_);
    UnblockPausedWriter();
  }
  CloseIo();
  CloseImpl();
}

void BaseEndpointChannel::CloseIo() {
  // Keep this method dedicated to reader and writer handling an nothing else.
  {
    // Do not take reader_mutex_ here: read may be in progress, and it will
    // deadlock. Calling Close() with Read() in progress will terminate the
    // IO and Read() will proceed normally (with Exception::kIo).
    Exception exception = reader_->Close();
    if (!exception.Ok()) {
      // Add logging.
    }
  }
  {
    // Do not take writer_mutex_ here: write may be in progress, and it will
    // deadlock. Calling Close() with Write() in progress will terminate the
    // IO and Write() will proceed normally (with Exception::kIo).
    Exception exception = writer_->Close();
    if (!exception.Ok()) {
      // Add logging.
    }
  }
}

void BaseEndpointChannel::Close(
    proto::connections::DisconnectionReason reason) {
  Close();
}

std::string BaseEndpointChannel::GetType() const {
  MutexLock crypto_lock(&crypto_mutex_);
  std::string subtype = IsEncryptionEnabledLocked() ? "ENCRYPTED_" : "";

  switch (GetMedium()) {
    case proto::connections::Medium::BLUETOOTH:
      return absl::StrCat(subtype, "BLUETOOTH");
    case proto::connections::Medium::BLE:
      return absl::StrCat(subtype, "BLE");
    case proto::connections::Medium::MDNS:
      return absl::StrCat(subtype, "MDNS");
    case proto::connections::Medium::WIFI_HOTSPOT:
      return absl::StrCat(subtype, "WIFI_HOTSPOT");
    case proto::connections::Medium::WIFI_LAN:
      return absl::StrCat(subtype, "WIFI_LAN");
    default:
      return "UNKNOWN";
  }
}

std::string BaseEndpointChannel::GetName() const { return channel_name_; }

void BaseEndpointChannel::EnableEncryption(
    std::shared_ptr<EncryptionContext> context) {
  MutexLock crypto_lock(&crypto_mutex_);
  crypto_context_ = context;
}

bool BaseEndpointChannel::IsPaused() const {
  MutexLock lock(&is_paused_mutex_);
  return is_paused_;
}

void BaseEndpointChannel::Pause() {
  MutexLock lock(&is_paused_mutex_);
  is_paused_ = true;
}

void BaseEndpointChannel::Resume() {
  MutexLock lock(&is_paused_mutex_);
  is_paused_ = false;
  is_paused_cond_.Notify();
}

absl::Time BaseEndpointChannel::GetLastReadTimestamp() const {
  MutexLock lock(&last_read_mutex_);
  return last_read_timestamp_;
}

bool BaseEndpointChannel::IsEncryptionEnabledLocked() const {
  return crypto_context_ != nullptr;
}

void BaseEndpointChannel::BlockUntilUnpaused() {
  // For more on how this works, see
  // https://docs.oracle.com/javase/tutorial/essential/concurrency/guardmeth.html
  while (is_paused_) {
    Exception wait_succeeded = is_paused_cond_.Wait();
    if (!wait_succeeded.Ok()) {
      return;
    }
  }
}

void BaseEndpointChannel::UnblockPausedWriter() {
  // For more on how this works, see
  // https://docs.oracle.com/javase/tutorial/essential/concurrency/guardmeth.html
  is_paused_ = false;
  is_paused_cond_.Notify();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
