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

#include "internal/platform/implementation/windows/nearby_client_socket.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "internal/flags/nearby_flags.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

NearbyClientSocket::NearbyClientSocket()
    : NearbyClientSocket(INVALID_SOCKET) {
}

NearbyClientSocket::NearbyClientSocket(SOCKET socket) : socket_(socket) {
  WSADATA wsa_data;
  int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (result != 0) {
    LOG(WARNING) << "WSAStartup failed with error " << result;
  }
  is_socket_initiated_ = (result == 0);
}

NearbyClientSocket::~NearbyClientSocket() {
  Close();
  if (is_socket_initiated_) {
    WSACleanup();
  }
}

bool NearbyClientSocket ::Connect(const SocketAddress& server_address) {
  if (!is_socket_initiated_) {
    LOG(WARNING) << "Windows socket is not initiated.";
    return false;
  }

  if (socket_ != INVALID_SOCKET) {
    LOG(ERROR) << "Socket is already connected.";
    return false;
  }
  socket_ = socket(server_address.dual_stack() ? AF_INET6 : AF_INET,
                   SOCK_STREAM, IPPROTO_TCP);

  if (socket_ == INVALID_SOCKET) {
    LOG(ERROR) << "Failed to get socket with error " << WSAGetLastError();
    return false;
  }
  if (server_address.dual_stack()) {
    // On Windows dual stack is not the default.
    // https://learn.microsoft.com/en-us/windows/win32/winsock/dual-stack-sockets#creating-a-dual-stack-socket
    DWORD v6_only = 0;
    if (setsockopt(socket_, IPPROTO_IPV6, IPV6_V6ONLY,
                   reinterpret_cast<const char*>(&v6_only),
                   sizeof(v6_only)) == SOCKET_ERROR) {
      LOG(WARNING) << "Failed to set IPV6_V6ONLY with error "
                   << WSAGetLastError();
    }
  }

  BOOL flag = TRUE;
  if (setsockopt(socket_, SOL_SOCKET, SO_KEEPALIVE,
                 reinterpret_cast<const char*>(&flag),
                 sizeof(flag)) == SOCKET_ERROR) {
    LOG(WARNING) << "Failed to set SO_KEEPALIVE with error "
                 << WSAGetLastError();
  }

  int send_buffer_size =
      static_cast<int>(NearbyFlags::GetInstance().GetInt64Flag(
          nearby::platform::config_package_nearby::nearby_platform_feature::
              kSocketSendBufferSize));
  if (setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
                 reinterpret_cast<char*>(&send_buffer_size),
                 sizeof(send_buffer_size)) == SOCKET_ERROR) {
    LOG(WARNING) << "Failed to set SO_SNDBUF with error " << WSAGetLastError();
  }

  flag = TRUE;
  setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flag),
             sizeof(flag));

  if (connect(socket_, server_address.address(), sizeof(sockaddr_storage)) ==
      SOCKET_ERROR) {
    LOG(ERROR) << "Failed to connect socket with error: " << WSAGetLastError();
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
    return false;
  }

  LOG(INFO) << "Client socket connected successfully";
  if (VLOG_IS_ON(1)) {
    SocketAddress local_address;
    int address_length = sizeof(sockaddr_storage);
    if (getsockname(socket_, local_address.address(), &address_length) !=
        SOCKET_ERROR) {
      VLOG(1) << "Connected to " << server_address.ToString() << " from "
              << local_address.ToString();
    }
  }

  return true;
}

ExceptionOr<ByteArray> NearbyClientSocket::Read(std::int64_t size) {
  if (socket_ == INVALID_SOCKET) {
    LOG(WARNING) << "Trying to read from an invalid socket.";
    return {Exception::kIo};
  }

  std::string buffer;
  buffer.resize(size);
  char* buffer_ptr = buffer.data();
  int64_t total_bytes_read = 0;

  // Loop until all requested bytes have been received.
  while (total_bytes_read < size) {
    int bytes_read = recv(
        /*s=*/socket_,
        /*buf=*/buffer_ptr + total_bytes_read,
        /*len=*/static_cast<int>(size - total_bytes_read),
        /*flags=*/0  // No special flags, just a standard receive.
    );

    if (bytes_read > 0) {
      // Successfully read some bytes.
      total_bytes_read += bytes_read;
    } else if (bytes_read == 0) {
      // The peer has performed a graceful shutdown.
      LOG(INFO) << "Socket closed gracefully by peer before all data was read.";
      return {Exception::kIo};
    } else {  // bytes_read == SOCKET_ERROR
      if (WSAGetLastError() == WSAEINTR) {
        VLOG(1) << "Interrupted while reading from socket.";
        continue;
      }
      // An error occurred during the receive operation.
      LOG(ERROR) << "Failed to receive data; recv failed with error: "
                 << WSAGetLastError();
      return {Exception::kIo};
    }
  }

  return ExceptionOr(ByteArray(std::move(buffer)));
}

ExceptionOr<size_t> NearbyClientSocket::Skip(size_t offset) {
  if (socket_ == INVALID_SOCKET) {
    LOG(WARNING) << "Trying to skip to an invalid socket.";
    return {Exception::kIo};
  }

  ExceptionOr<ByteArray> data = Read(offset);
  if (data.ok()) {
    return ExceptionOr(data.result().size());
  }

  LOG(ERROR) << "Failed to skip data " << WSAGetLastError();
  return {Exception::kIo};
}

Exception NearbyClientSocket::Write(const ByteArray& data) {
  if (socket_ == INVALID_SOCKET) {
    LOG(WARNING) << "Trying to write to an invalid socket.";
    return {Exception::kIo};
  }

  int count = 0;
  while (count < data.size()) {
    int result = send(/*s=*/socket_, /*buf=*/data.data() + count,
                      /*len=*/data.size() - count, /*flags*/ 0);
    if (result == SOCKET_ERROR) {
      LOG(ERROR) << "Failed to send data " << WSAGetLastError();
      return {Exception::kIo};
    }
    count += result;
  }

  return {Exception::kSuccess};
}

Exception NearbyClientSocket::Flush() {
  // Socket doesn't support flush.
  return {Exception::kSuccess};
}

Exception NearbyClientSocket::Close() {
  if (socket_ == INVALID_SOCKET) {
    VLOG(1) << "Socket already closed.";
    return {Exception::kSuccess};
  }

  shutdown(socket_, SD_BOTH);
  closesocket(socket_);
  socket_ = INVALID_SOCKET;

  LOG(INFO) << "Socket closed.";

  return {Exception::kSuccess};
}

}  // namespace nearby::windows
