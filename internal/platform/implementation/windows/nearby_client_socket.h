// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_NEARBY_CLIENT_SOCKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_NEARBY_CLIENT_SOCKET_H_

#include <winsock2.h>

#include <cstddef>
#include <cstdint>

#include "absl/base/nullability.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby::windows {

// Socket wrapper class.
// This class is thread-compatible.
class NearbyClientSocket {
 public:
  // Creates a socket not bound to underlying platform implementation.
  // `Connect` must be called to create a platform socket.
  NearbyClientSocket();
  // Creates a socket with the given platform socket.
  // Calling `Connect` on this socket will fail.
  explicit NearbyClientSocket(SOCKET socket);
  ~NearbyClientSocket();

  bool Connect(const SocketAddress& server_address);
  ExceptionOr<ByteArray> Read(std::int64_t size);
  ExceptionOr<size_t> Skip(size_t offset);
  Exception Write(const ByteArray& data);
  Exception Flush();
  Exception Close();

 private:
  bool is_socket_initiated_ = false;
  SOCKET socket_ = INVALID_SOCKET;
};

// A simple wrapper to handle input stream of socket
class SocketInputStream : public InputStream {
 public:
  explicit SocketInputStream(NearbyClientSocket* absl_nonnull client_socket)
      : client_socket_(client_socket) {}
  ~SocketInputStream() override = default;

  ExceptionOr<ByteArray> Read(std::int64_t size) override {
    return client_socket_->Read(size);
  }
  ExceptionOr<size_t> Skip(size_t offset) override {
    return client_socket_->Skip(offset);
  }
  Exception Close() override { return client_socket_->Close(); }

 private:
  NearbyClientSocket* absl_nonnull const client_socket_;
};

// A simple wrapper to handle output stream of socket
class SocketOutputStream : public OutputStream {
 public:
  explicit SocketOutputStream(NearbyClientSocket* absl_nonnull client_socket)
      : client_socket_(client_socket) {}
  ~SocketOutputStream() override = default;

  Exception Write(const ByteArray& data) override {
    return client_socket_->Write(data);
  }
  Exception Flush() override { return client_socket_->Flush(); }
  Exception Close() override { return client_socket_->Close(); }

 private:
  NearbyClientSocket* absl_nonnull const client_socket_;
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_NEARBY_CLIENT_SOCKET_H_
