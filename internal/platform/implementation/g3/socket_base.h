// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_G3_SOCKET_BASE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_G3_SOCKET_BASE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/pipe.h"

namespace nearby {
namespace g3 {

// Common base for BT, BLE and Wifi socket implementations.
class SocketBase {
 public:
  SocketBase() { std::tie(input_for_remote_, output_) = CreatePipe(); }
  virtual ~SocketBase() {
    absl::MutexLock lock(&mutex_);
    DoClose();
  }

  // Connects to another Socket, to form a functional low-level
  // channel. From this point on, and until Close is called, connection exists.
  void Connect(SocketBase& other) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    remote_socket_ = &other;
    input_ = std::move(other.input_for_remote_);
  }

  // Returns the InputStream of this connected socket.
  InputStream& GetInputStream() { return input_proxy_; }

  // Returns the OutputStream of this connected socket.
  // This stream is for local side to write.
  OutputStream& GetOutputStream() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return *output_;
  }

  // Returns true if connection exists to the (possibly closed) remote socket.
  bool IsConnected() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return IsConnectedLocked();
  }

  // Returns true if socket is closed.
  bool IsClosed() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return closed_;
  }

  // Closes both input and output streams, marks Socket as closed.
  // After this call object should be treated as not connected.
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    DoClose();
    return {Exception::kSuccess};
  }

  SocketBase* GetRemoteSocket() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return remote_socket_;
  }

 protected:
  mutable absl::Mutex mutex_;

 private:
  void DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    if (!closed_) {
      remote_socket_ = nullptr;
      // The client can hold references to `output_` and `input_` streams. We
      // can close them but we cannot destroy them.
      output_->Close();
      if (input_) {
        input_->Close();
      }
      // The client does not hold a reference to `input_for_remote_`, so we can
      // destroy it. Connecting to this socket will fail after that.
      input_for_remote_.reset();
      closed_ = true;
    }
  }

  // Returns true if connection exists to the (possibly closed) remote socket.
  bool IsConnectedLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return input_ != nullptr;
  }

  class InputProxyStream : public InputStream {
   public:
    explicit InputProxyStream(SocketBase* socket) : socket_(socket) {}
    ExceptionOr<ByteArray> Read(std::int64_t size) override {
      if (!socket_->IsConnected()) {
        return ExceptionOr<ByteArray>(Exception::kIo);
      }
      return socket_->input_->Read(size);
    }
    ExceptionOr<size_t> Skip(size_t offset) override {
      if (!socket_->IsConnected()) {
        return ExceptionOr<size_t>(Exception::kIo);
      }
      return socket_->input_->Skip(offset);
    }
    Exception Close() override {
      if (!socket_->IsConnected()) {
        return {Exception::kIo};
      }
      return socket_->input_->Close();
    }

   private:
    SocketBase* socket_;
  };
  InputProxyStream input_proxy_{this};

  // Output stream is initialized by constructor, it remains always valid. It
  // represents output part of a local socket. Input stream of a local socket
  // comes from the peer socket, after connection.
  std::unique_ptr<OutputStream> output_;
  std::unique_ptr<InputStream> input_;
  // `input_for_remote_` is the other end of the pipe formed with `output_`. We
  // give this stream to the remote socket when they connect to us, and it
  // becomes their `input_` stream.
  std::unique_ptr<InputStream> input_for_remote_;
  SocketBase* remote_socket_ ABSL_GUARDED_BY(mutex_) = nullptr;
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace g3
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_G3_SOCKET_BASE_H_
