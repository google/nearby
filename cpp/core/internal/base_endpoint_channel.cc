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

#include "core/internal/base_endpoint_channel.h"

#include <cassert>

#include "platform/api/platform.h"
#include "platform/synchronized.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

using Platform = platform::ImplementationPlatform;

std::int32_t bytesToInt(ConstPtr<ByteArray> bytes) {
  const char* int_bytes = bytes->getData();

  std::int32_t result = 0;
  result |= (static_cast<std::int32_t>(int_bytes[0]) & 0x0FF) << 24;
  result |= (static_cast<std::int32_t>(int_bytes[1]) & 0x0FF) << 16;
  result |= (static_cast<std::int32_t>(int_bytes[2]) & 0x0FF) << 8;
  result |= (static_cast<std::int32_t>(int_bytes[3]) & 0x0FF);

  return result;
}

ConstPtr<ByteArray> intToBytes(std::int32_t value) {
  char int_bytes[sizeof(std::int32_t)];
  int_bytes[0] = static_cast<char>((value >> 24) & 0x0FF);
  int_bytes[1] = static_cast<char>((value >> 16) & 0x0FF);
  int_bytes[2] = static_cast<char>((value >> 8) & 0x0FF);
  int_bytes[3] = static_cast<char>((value)&0x0FF);

  return MakeConstPtr(new ByteArray(int_bytes, sizeof(int_bytes)));
}

ExceptionOr<ConstPtr<ByteArray>> readExactly(Ptr<InputStream> reader,
                                             std::int64_t size) {
  string buffer;
  std::int64_t remaining_size = size;

  while (remaining_size > 0) {
    ExceptionOr<ConstPtr<ByteArray>> read_bytes = reader->read(remaining_size);
    if (!read_bytes.ok()) {
      if (Exception::IO == read_bytes.exception()) {
        return ExceptionOr<ConstPtr<ByteArray>>(read_bytes.exception());
      }
    }
    // Avoid leaks.
    ScopedPtr<ConstPtr<ByteArray>> scoped_read_bytes(read_bytes.result());

    // In Java, EOFException is a sub-variant of IOException.
    if (scoped_read_bytes.isNull() || scoped_read_bytes->size() == 0) {
      return ExceptionOr<ConstPtr<ByteArray>>(Exception::IO);
    }

    buffer.append(scoped_read_bytes->getData(), scoped_read_bytes->size());
    remaining_size -= scoped_read_bytes->size();
  }

  return ExceptionOr<ConstPtr<ByteArray>>(
      MakeConstPtr(new ByteArray(buffer.data(), buffer.size())));
}

ExceptionOr<std::int32_t> readInt(Ptr<InputStream> reader) {
  ExceptionOr<ConstPtr<ByteArray>> read_bytes =
      readExactly(reader, sizeof(std::int32_t));
  if (!read_bytes.ok()) {
    if (Exception::IO == read_bytes.exception()) {
      return ExceptionOr<std::int32_t>(read_bytes.exception());
    }
  }
  // Avoid leaks.
  ScopedPtr<ConstPtr<ByteArray>> scoped_read_bytes(read_bytes.result());

  return ExceptionOr<std::int32_t>(bytesToInt(scoped_read_bytes.get()));
}

Exception::Value writeInt(Ptr<OutputStream> writer, std::int32_t value) {
  return writer->write(intToBytes(value));
}

}  // namespace

// TODO(b/150763574): Move implementatiopn to header or .inc file.
BaseEndpointChannel::BaseEndpointChannel(absl::string_view channel_name,
                                         Ptr<InputStream> reader,
                                         Ptr<OutputStream> writer)
    : last_read_timestamp_(-1),
      channel_name_(channel_name),
      system_clock_(Platform::createSystemClock()),
      reader_lock_(Platform::createLock()),
      reader_(reader),
      writer_lock_(Platform::createLock()),
      writer_(writer),
      encryption_context_(Platform::createAtomicReference(
          Ptr<securegcm::D2DConnectionContextV1>())),
      is_paused_lock_(Platform::createLock()),
      is_paused_condition_variable_(
          Platform::createConditionVariable(is_paused_lock_.get())),
      is_paused_(Platform::createAtomicBoolean(false)) {}

BaseEndpointChannel::~BaseEndpointChannel() {
  // WARNING: Make sure to never access reader_ and writer_ from here.
  //
  // They're owned by the specialized *Socket classes that are in turn
  // owned by the *EndpointChannel children of this class, so by this point,
  // they've been destroyed and now point to invalid memory.
  //
  // "Ugh!" is right -- this won't be a problem once we have a standardized
  // Socket interface we can hold up in this class (instead of holding
  // specialized implementations of that hypothetical interface in each child
  // of this class).
}

ExceptionOr<ConstPtr<ByteArray>> BaseEndpointChannel::read() {
  Synchronized s(reader_lock_.get());

  ExceptionOr<std::int32_t> read_int = readInt(reader_);
  if (!read_int.ok()) {
    if (Exception::IO == read_int.exception()) {
      return ExceptionOr<ConstPtr<ByteArray>>(read_int.exception());
    }
  }

  if (read_int.result() < 0) {
    return ExceptionOr<ConstPtr<ByteArray>>(Exception::IO);
  } else if (read_int.result() > kMaxAllowedReadBytes) {
    return ExceptionOr<ConstPtr<ByteArray>>(Exception::IO);
  }

  ExceptionOr<ConstPtr<ByteArray>> read_bytes =
      readExactly(reader_, read_int.result());
  if (!read_bytes.ok()) {
    if (Exception::IO == read_bytes.exception()) {
      return ExceptionOr<ConstPtr<ByteArray>>(read_bytes.exception());
    }
  }

  // This should be ScopedPtr usually, but because of the unique requirement of
  // reassigning this variable when encryption is enabled, we can't make use of
  // the power of ScopedPtr, and instead have to do manual memory management.
  ConstPtr<ByteArray> read_bytes_result = read_bytes.result();

  // If encryption is enabled, decode the message.
  if (isEncryptionEnabled()) {
    std::unique_ptr<string> decoded_bytes =
        encryption_context_->get()->DecodeMessageFromPeer(
            string(read_bytes_result->getData(), read_bytes_result->size()));
    // Now that we are done using read_bytes_result, we should unconditionally
    // destroy it, because we either reassign to the value of decoded_bytes, or
    // short-circuit out of here on error.
    read_bytes_result.destroy();
    if (decoded_bytes == nullptr) {
      return ExceptionOr<ConstPtr<ByteArray>>(
          Exception::INVALID_PROTOCOL_BUFFER);
    }
    read_bytes_result = MakeConstPtr(
        new ByteArray(decoded_bytes->data(), decoded_bytes->size()));
  }

  last_read_timestamp_ = system_clock_->elapsedRealtime();
  return ExceptionOr<ConstPtr<ByteArray>>(read_bytes_result);
}

Exception::Value BaseEndpointChannel::write(ConstPtr<ByteArray> data) {
  Synchronized s(writer_lock_.get());

  // Avoid leaks.
  ScopedPtr<ConstPtr<ByteArray>> scoped_data(data);

  if (isPaused()) {
    blockUntilUnpaused();
  }

  ConstPtr<ByteArray> data_to_write;
  // If encryption is enabled, encode the message.
  if (isEncryptionEnabled()) {
    std::unique_ptr<string> message =
        encryption_context_->get()->EncodeMessageToPeer(
            string(scoped_data->getData(), scoped_data->size()));
    assert(message != nullptr);
    data_to_write =
        MakeConstPtr(new ByteArray(message->data(), message->size()));
  } else {
    // Else, just make data_to_write point to the passed-in data.
    data_to_write = scoped_data.release();
  }
  // Avoid leaks.
  ScopedPtr<ConstPtr<ByteArray>> scoped_data_to_write(data_to_write);

  Exception::Value write_exception = writeInt(
      writer_, static_cast<std::int32_t>(scoped_data_to_write->size()));
  if (Exception::NONE != write_exception) {
    if (Exception::IO == write_exception) {
      return write_exception;
    }
  }

  write_exception = writer_->write(scoped_data_to_write.release());
  if (Exception::NONE != write_exception) {
    if (Exception::IO == write_exception) {
      return write_exception;
    }
  }

  Exception::Value flush_exception = writer_->flush();
  if (Exception::NONE != flush_exception) {
    if (Exception::IO == flush_exception) {
      return flush_exception;
    }
  }

  return Exception::NONE;
}

void BaseEndpointChannel::close() {
  //                      WARNING WARNING WARNING
  //
  // This block deviates from the corresponding Java code.
  //
  // In the corresponding Java code, close() calls
  // close(proto::connections::DisconnectionReason) while here we do the
  // opposite. This is because proto::connections::DisconnectionReason can be
  // null in Java but not in C++.
  Exception::Value reader_close_exception = reader_->close();
  if (Exception::NONE != reader_close_exception) {
    if (Exception::IO == reader_close_exception) {
      // Add logging.
    }
  }
  Exception::Value writer_close_exception = writer_->close();
  if (Exception::NONE != writer_close_exception) {
    if (Exception::IO == writer_close_exception) {
      // Add logging.
    }
  }

  closeImpl();

  // TODO(tracyzhou): Add logging.
}

void BaseEndpointChannel::close(
    proto::connections::DisconnectionReason reason) {
  //                      WARNING WARNING WARNING
  //
  // This block deviates from the corresponding Java code.
  // Look at the corresponding block in the close() method above for details on
  // the deviation.
  close();

  // TODO(tracyzhou): Add logging.
}

string BaseEndpointChannel::getType() {
  string subtype = isEncryptionEnabled() ? "ENCRYPTED_" : "";
  switch (getMedium()) {
    case proto::connections::Medium::BLUETOOTH:
      return subtype + "BLUETOOTH";
    case proto::connections::Medium::BLE:
      return subtype + "BLE";
    case proto::connections::Medium::MDNS:
      return subtype + "MDNS";
    case proto::connections::Medium::WIFI_HOTSPOT:
      return subtype + "WIFI_HOTSPOT";
    case proto::connections::Medium::WIFI_LAN:
      return subtype + "WIFI_LAN";
    default:
      return "UNKNOWN";
  }
}

string BaseEndpointChannel::getName() { return channel_name_; }

void BaseEndpointChannel::enableEncryption(
    Ptr<securegcm::D2DConnectionContextV1> encryption_context) {
  assert(!encryption_context.isNull());
  encryption_context_->set(encryption_context);
}

bool BaseEndpointChannel::isPaused() { return is_paused_->get(); }

void BaseEndpointChannel::pause() { is_paused_->set(true); }

void BaseEndpointChannel::resume() {
  is_paused_->set(false);
  unblockPausedWriter();
}

std::int64_t BaseEndpointChannel::getLastReadTimestamp() {
  return last_read_timestamp_;
}

bool BaseEndpointChannel::isEncryptionEnabled() {
  return !encryption_context_->get().isNull();
}

void BaseEndpointChannel::unblockPausedWriter() {
  Synchronized s(is_paused_lock_.get());

  // Notify to tell the thread calling wait() to check again.
  // NOTE: There is only ever one thread blocked by wait() at a time, because
  // EndpointChannel.write(Ptr<ByteArray>) is synchronized on writer. That means
  // the first thread to call write(byte[]) will be blocked via
  // blockUntilUnpaused() and all future threads will be blocked via
  // synchronized(writer_lock_).
  is_paused_condition_variable_->notify();
}

void BaseEndpointChannel::blockUntilUnpaused() {
  Synchronized s(is_paused_lock_.get());

  // For more on how this works, see
  // https://docs.oracle.com/javase/tutorial/essential/concurrency/guardmeth.html
  while (is_paused_->get()) {
    Exception::Value wait_succeeded = is_paused_condition_variable_->wait();
    if (Exception::NONE != wait_succeeded) {
      if (Exception::INTERRUPTED == wait_succeeded) {
        // If we were interrupted, pass the interrupt up the stack and then exit
        // immediately.
        // Thread.currentThread().interrupt();
        return;
      }
    }
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
