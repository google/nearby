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
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
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
  Close();
  if (is_socket_initiated_) {
    WSACleanup();
  }
}

bool NearbyServerSocket::Listen(const SocketAddress& address) {
  VLOG(1) << "Listen to socket at " << address.ToString();
  if (!is_socket_initiated_) {
    LOG(ERROR) << "Windows socket is not initiated.";
    return false;
  }

  absl::MutexLock lock( mutex_ );
  socket_ = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
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

  // On Windows dual stack is not the default.
  // https://learn.microsoft.com/en-us/windows/win32/winsock/dual-stack-sockets#creating-a-dual-stack-socket
  DWORD v6_only = 0;
  if (setsockopt(socket_, IPPROTO_IPV6, IPV6_V6ONLY,
                  reinterpret_cast<const char*>(&v6_only),
                  sizeof(v6_only)) == SOCKET_ERROR) {
    LOG(WARNING) << "Failed to set IPV6_V6ONLY with error "
                  << WSAGetLastError();
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

  SocketAddress dual_stack_address = address.ToMappedIPv6();
  if (bind(socket_, dual_stack_address.address(), sizeof(sockaddr_storage)) ==
      SOCKET_ERROR) {
    LOG(ERROR) << "Failed to bind socket with error " << WSAGetLastError();
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
    return false;
  }

  SocketAddress local_address;
  int address_length = sizeof(sockaddr_storage);
  if (getsockname(socket_, local_address.address(), &address_length) ==
      SOCKET_ERROR) {
    LOG(ERROR) << "Failed to get socket name with error " << WSAGetLastError();
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
    return false;
  }

  port_ = local_address.port();

  VLOG(1) << "Bound to " << local_address.ToString();

  if (::listen(socket_, /*backlog=*/SOMAXCONN) == SOCKET_ERROR) {
    LOG(ERROR) << "Failed to listen socket with error " << WSAGetLastError();
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
    return false;
  }

  return true;
}

std::unique_ptr<NearbyClientSocket> NearbyServerSocket::Accept() {
  LOG(INFO) << "Accept is called on NearbyServerSocket.";
  SOCKET socket = INVALID_SOCKET;
  {
    absl::MutexLock lock( mutex_ );
    if (!is_socket_initiated_ || socket_ == INVALID_SOCKET) {
      LOG(WARNING) << "Windows socket is not initiated";
      return nullptr;
    }
    socket = socket_;
  }

  SocketAddress peer_address;
  int peer_address_length = sizeof(sockaddr_storage);
  // Release lock before calling accept.  Otherwise the accept call blocks and
  // prevents other calls from using the socket.
  SOCKET client_socket = accept(socket, peer_address.address(),
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
  absl::AnyInvocable<void()> close_callback;
  {
    absl::MutexLock lock( mutex_ );
    if (socket_ != INVALID_SOCKET) {
      if (closesocket(/*s=*/socket_) == SOCKET_ERROR) {
        LOG(WARNING) << "Close socket failed with error:" << WSAGetLastError();
      }
      socket_ = INVALID_SOCKET;
      close_callback = std::move(close_notifier_);
    }
  }
  if (close_callback) {
    close_callback();
  }
  LOG(INFO) << "Closed NearbyServerSocket.";
  return true;
}

void NearbyServerSocket::SetCloseNotifier(absl::AnyInvocable<void()> notifier) {
  absl::MutexLock lock( mutex_ );
  close_notifier_ = std::move(notifier);
}

}  // namespace nearby::windows
