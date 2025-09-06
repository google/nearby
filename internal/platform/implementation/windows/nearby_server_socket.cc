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

bool NearbyServerSocket::Listen(const std::string& ip_address, int port) {
  VLOG(1) << "Listen to socket at " << ip_address << ":" << port;
  if (!is_socket_initiated_) {
    LOG(ERROR) << "Windows socket is not initiated.";
    return false;
  }

  socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_ == INVALID_SOCKET) {
    LOG(ERROR) << "Failed to create socket.";
    return false;
  }

  BOOL flag = TRUE;
  if (setsockopt(/*s=*/socket_, /*level=*/SOL_SOCKET, /*optname=*/SO_KEEPALIVE,
                 /*optval=*/(const char*)&flag,
                 /*optlen=*/sizeof(flag)) == SOCKET_ERROR) {
    LOG(WARNING) << "Failed to set SO_KEEPALIVE with error "
                 << WSAGetLastError();
  }

  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  if (ip_address.empty()) {
    serv_addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    serv_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());
  }
  // Set REUSEADDR if a specific port is needed.
  if (port != 0) {
    BOOL flag = TRUE;
    if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&flag,
                   sizeof(flag)) == SOCKET_ERROR) {
      LOG(WARNING) << "Failed to set SO_REUSEADDR with error "
                   << WSAGetLastError();
    }
  }

  if (bind(/*s=*/socket_, /*addr=*/(struct sockaddr*)&serv_addr,
           /*namelen=*/sizeof(serv_addr)) == SOCKET_ERROR) {
    LOG(ERROR) << "Failed to bind socket with error " << WSAGetLastError();
    closesocket(socket_);
    return false;
  }

  sockaddr_in local_address;
  int address_length = sizeof(local_address);
  if (getsockname(/*s=*/socket_, (/*name=*/SOCKADDR*)&local_address,
                  /*namelen=*/&address_length) == SOCKET_ERROR) {
    LOG(ERROR) << "Failed to get socket name with error " << WSAGetLastError();
    closesocket(socket_);
    return false;
  }

  ip_address_ = ip_address;
  port_ = ntohs(local_address.sin_port);

  VLOG(1) << "Bound to " << ip_address_ << ":" << port_;

  if (::listen(/*s=*/socket_, /*backlog=*/SOMAXCONN) == SOCKET_ERROR) {
    LOG(ERROR) << "Failed to listen socket with error " << WSAGetLastError();
    closesocket(socket_);
    return false;
  }

  return true;
}

std::unique_ptr<NearbyClientSocket> NearbyServerSocket::Accept() {
  VLOG(1) << "Accept is called on NearbyServerSocket.";
  if (!is_socket_initiated_) {
    LOG(WARNING) << "Windows socket is not initiated";
    return nullptr;
  }

  sockaddr_in peer_address;
  int peer_address_length = sizeof(peer_address);

  SOCKET client_socket =
      accept(/*s=*/socket_, /*addr=*/(SOCKADDR*)&peer_address,
             /*addrlen=*/&peer_address_length);
  if (client_socket == INVALID_SOCKET) {
    LOG(ERROR) << "Failed to accept socket with error: " << WSAGetLastError();
    return nullptr;
  }

  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(peer_address.sin_addr), client_ip, INET_ADDRSTRLEN);
  int client_port = ntohs(peer_address.sin_port);

  LOG(INFO) << "Accepted remote device " << client_ip << ":" << client_port;
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
