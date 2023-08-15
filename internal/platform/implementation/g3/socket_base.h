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

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/g3/pipe.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace g3 {

// Common base for BT, BLE and Wifi socket implementations.
class SocketBase {
 public:
  virtual ~SocketBase() {
    absl::MutexLock lock(&mutex_);
    DoClose();
  }

  // Connects to another Socket, to form a functional low-level
  // channel. From this point on, and until Close is called, connection exists.
  void Connect(SocketBase& other) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    remote_socket_ = &other;
    input_ = other.output_;
  }

  // Returns the InputStream of this connected socket.
  InputStream& GetInputStream() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    if (IsConnectedLocked()) {
      return input_->GetInputStream();
    }
    return invalid_input_stream_;
  }

  // Returns the OutputStream of this connected socket.
  // This stream is for local side to write.
  OutputStream& GetOutputStream() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return output_->GetOutputStream();
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
      output_->GetOutputStream().Close();
      output_->GetInputStream().Close();
      if (IsConnectedLocked()) {
        input_->GetOutputStream().Close();
        input_->GetInputStream().Close();
        input_.reset();
      }
      closed_ = true;
    }
  }

  // Returns true if connection exists to the (possibly closed) remote socket.
  bool IsConnectedLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return input_ != nullptr;
  }

  class InvalidInputStream : public InputStream {
   public:
    ExceptionOr<ByteArray> Read(std::int64_t size) override {
      return ExceptionOr<ByteArray>(Exception::kIo);
    }
    ExceptionOr<size_t> Skip(size_t offset) override {
      return ExceptionOr<size_t>(Exception::kIo);
    }
    Exception Close() override { return {Exception::kIo}; }
  };
  // Returned to the caller if the remote socket is destroyed.
  InvalidInputStream invalid_input_stream_;

  // Output pipe is initialized by constructor, it remains always valid, until
  // it is closed. it represents output part of a local socket. Input part of a
  // local socket comes from the peer socket, after connection.
  std::shared_ptr<Pipe> output_{new Pipe};
  std::shared_ptr<Pipe> input_;
  SocketBase* remote_socket_ ABSL_GUARDED_BY(mutex_) = nullptr;
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace g3
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_G3_SOCKET_BASE_H_
