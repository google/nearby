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
#include "internal/platform/logging.h"

namespace nearby::windows {

NearbyClientSocket::NearbyClientSocket() {
  WSADATA wsa_data;
  int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (result != 0) {
    LOG(WARNING) << "WSAStartup failed with error " << result;
  }

  is_socket_initiated_ = (result == 0);
}

NearbyClientSocket::NearbyClientSocket(SOCKET socket) : socket_(socket) {}

NearbyClientSocket::~NearbyClientSocket() {
  if (is_socket_initiated_) {
    WSACleanup();
  }
}

bool NearbyClientSocket ::Connect(const std::string& ip_address, int port) {
  if (!is_socket_initiated_) {
    LOG(WARNING) << "Windows socket is not initiated.";
    return false;
  }

  if (socket_ != INVALID_SOCKET) {
    LOG(ERROR) << "Socket is already connected.";
    return false;
  }

  socket_ =
      socket(/*af=*/AF_INET, /*type=*/SOCK_STREAM, /*protocol=*/IPPROTO_TCP);

  if (socket_ == INVALID_SOCKET) {
    LOG(ERROR) << "Failed to get socket with error " << WSAGetLastError();
    return false;
  }

  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());

  BOOL flag = TRUE;
  if (setsockopt(/*s=*/socket_, /*level=*/SOL_SOCKET, /*optname=*/SO_KEEPALIVE,
                 /*optval=*/(const char*)&flag,
                 /*optlen=*/sizeof(flag)) == SOCKET_ERROR) {
    LOG(WARNING) << "Failed to set SO_KEEPALIVE with error "
                 << WSAGetLastError();
  }

  int send_buffer_size =
      static_cast<int>(NearbyFlags::GetInstance().GetInt64Flag(
          nearby::platform::config_package_nearby::nearby_platform_feature::
              kSocketSendBufferSize));
  if (setsockopt(/*s=*/socket_, /*level=*/SOL_SOCKET, /*optname=*/SO_SNDBUF,
                 /*optval=*/(char*)&send_buffer_size,
                 /*optlen=*/sizeof(send_buffer_size)) == SOCKET_ERROR) {
    LOG(WARNING) << "Failed to set SO_SNDBUF with error " << WSAGetLastError();
  }

  flag = TRUE;
  setsockopt(/*s=*/socket_, /*level=*/IPPROTO_TCP, /*optname=*/TCP_NODELAY,
             /*optval=*/(char*)&flag, /*optlen=*/sizeof(flag));

  if (connect(/*s=*/socket_, /*name=*/(struct sockaddr*)&serv_addr,
              /*namelen=*/sizeof(serv_addr)) == SOCKET_ERROR) {
    LOG(ERROR) << "Failed to connect socket with error: " << WSAGetLastError();
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
    return false;
  }

  sockaddr_in local_address;
  int address_length = sizeof(local_address);
  if (getsockname(/*s=*/socket_, /*name=*/(SOCKADDR*)&local_address,
                  /*namelen=*/&address_length) != SOCKET_ERROR) {
    // Extract the IP address and port
    char local_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(local_address.sin_addr), local_ip, INET_ADDRSTRLEN);
    int local_port = ntohs(local_address.sin_port);

    LOG(INFO) << "Connected to " << ip_address << ":" << port << " from "
              << local_ip << ":" << local_port;
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
    LOG(WARNING) << "Trying to close an invalid socket.";
    return {Exception::kIo};
  }

  shutdown(socket_, SD_BOTH);
  closesocket(socket_);
  socket_ = INVALID_SOCKET;

  LOG(INFO) << "Socket closed.";

  return {Exception::kSuccess};
}

}  // namespace nearby::windows
