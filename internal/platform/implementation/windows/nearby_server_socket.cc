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

#include "internal/platform/implementation/windows/nearby_server_socket.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <memory>
#include <string>

#include "internal/platform/implementation/windows/nearby_client_socket.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

NearbyServerSocket::NearbyServerSocket() {
  WSADATA wsa_data;
  int result =
      WSAStartup(/*wVersionRequired=*/MAKEWORD(2, 2), /*lpWSAData=*/&wsa_data);
  if (result != 0) {
    LOG(WARNING) << "WSAStartup failed with error:" << result;
  }

  is_socket_initiated_ = (result == 0);
}

NearbyServerSocket::~NearbyServerSocket() {
  if (is_socket_initiated_) {
    WSACleanup();
  }
}

bool NearbyServerSocket::Listen(const SocketAddress& address) {
  VLOG(1) << "Listen to socket at " << address.ToString();
  LOG(INFO) << "Server socket dual stack support: " << address.dual_stack();
  if (!is_socket_initiated_) {
    LOG(ERROR) << "Windows socket is not initiated.";
    return false;
  }

  socket_ = socket(address.dual_stack() ? AF_INET6 : AF_INET, SOCK_STREAM,
                   IPPROTO_TCP);
  if (socket_ == INVALID_SOCKET) {
    LOG(ERROR) << "Failed to create socket.";
    return false;
  }

  BOOL flag = TRUE;
  if (setsockopt(socket_, SOL_SOCKET, SO_KEEPALIVE,
                 reinterpret_cast<const char*>(&flag),
                 sizeof(flag)) == SOCKET_ERROR) {
    LOG(WARNING) << "Failed to set SO_KEEPALIVE with error "
                 << WSAGetLastError();
  }

  if (address.dual_stack()) {
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
  // Set REUSEADDR if a specific port is needed.
  if (address.port() != 0) {
    BOOL flag = TRUE;
    if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&flag),
                   sizeof(flag)) == SOCKET_ERROR) {
      LOG(WARNING) << "Failed to set SO_REUSEADDR with error "
                   << WSAGetLastError();
    }
  }

  if (bind(socket_, address.address(), sizeof(sockaddr_storage)) ==
      SOCKET_ERROR) {
    LOG(ERROR) << "Failed to bind socket with error " << WSAGetLastError();
    closesocket(socket_);
    return false;
  }

  SocketAddress local_address(address.dual_stack());
  int address_length = sizeof(sockaddr_storage);
  if (getsockname(socket_, local_address.address(), &address_length) ==
      SOCKET_ERROR) {
    LOG(ERROR) << "Failed to get socket name with error " << WSAGetLastError();
    closesocket(socket_);
    return false;
  }

  port_ = local_address.port();

  VLOG(1) << "Bound to " << local_address.ToString();

  if (::listen(socket_, /*backlog=*/SOMAXCONN) == SOCKET_ERROR) {
    LOG(ERROR) << "Failed to listen socket with error " << WSAGetLastError();
    closesocket(socket_);
    return false;
  }

  return true;
}

std::unique_ptr<NearbyClientSocket> NearbyServerSocket::Accept() {
  LOG(INFO) << "Accept is called on NearbyServerSocket.";
  if (!is_socket_initiated_) {
    LOG(WARNING) << "Windows socket is not initiated";
    return nullptr;
  }

  SocketAddress peer_address;
  int peer_address_length = sizeof(sockaddr_storage);

  SOCKET client_socket = accept(socket_, peer_address.address(),
                                /*addrlen=*/&peer_address_length);
  if (client_socket == INVALID_SOCKET) {
    LOG(ERROR) << "Failed to accept socket with error: " << WSAGetLastError();
    return nullptr;
  }

  LOG(INFO) << "Accepted remote device.";
  VLOG(1) << "Remote device address: " << peer_address.ToString();
  return std::make_unique<NearbyClientSocket>(client_socket);
}

bool NearbyServerSocket::Close() {
  bool result = true;
  if (socket_ != INVALID_SOCKET) {
    if (shutdown(/*s=*/socket_, /*how=*/SD_BOTH) == SOCKET_ERROR) {
      LOG(WARNING) << "Shutdown failed with error:" << WSAGetLastError();
      result = false;
    }

    if (closesocket(/*s=*/socket_) == SOCKET_ERROR) {
      LOG(WARNING) << "Close socket failed with error:" << WSAGetLastError();
      result = false;
    }

    socket_ = INVALID_SOCKET;
  }

  LOG(INFO) << "Closed NearbyServerSocket.";
  return result;
}

}  // namespace nearby::windows
