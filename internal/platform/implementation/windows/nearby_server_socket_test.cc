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

#include "gtest/gtest.h"
#include "internal/platform/implementation/windows/socket_address.h"

namespace nearby::windows {
namespace {

SOCKET CreateSocket() {
  SOCKET socket_handle = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
  DWORD v6_only = 0;
  EXPECT_NE(
      setsockopt(socket_handle, IPPROTO_IPV6, IPV6_V6ONLY,
                 reinterpret_cast<const char*>(&v6_only), sizeof(v6_only)),
      SOCKET_ERROR);
  return socket_handle;
}

TEST(NearbyServerSocketTest, Listen) {
  NearbyServerSocket server_socket;
  SocketAddress address(/*dual_stack=*/true);
  SocketAddress::FromString(address, "", 0);
  EXPECT_TRUE(server_socket.Listen(address));
  EXPECT_NE(server_socket.GetPort(), 0);
}

TEST(NearbyServerSocketTest, Accept) {
  NearbyServerSocket server_socket;
  SocketAddress address(/*dual_stack=*/true);
  SocketAddress::FromString(address, "", 0);
  EXPECT_TRUE(server_socket.Listen(address));
  SOCKET socket_handle = CreateSocket();
  EXPECT_NE(socket_handle, INVALID_SOCKET);
  SocketAddress server_address(/*dual_stack=*/true);
  SocketAddress::FromString(server_address, "::1", server_socket.GetPort());
  EXPECT_NE(connect(socket_handle, server_address.address(),
                    sizeof(sockaddr_storage)),
            SOCKET_ERROR);

  auto client_socket = server_socket.Accept();
  ASSERT_NE(client_socket, nullptr);
  EXPECT_TRUE(server_socket.Close());
}

TEST(NearbyServerSocketTest, CloseNotifier) {
  NearbyServerSocket server_socket;
  SocketAddress address(/*dual_stack=*/true);
  SocketAddress::FromString(address, "", 0);
  EXPECT_TRUE(server_socket.Listen(address));
  bool is_closed = false;
  server_socket.SetCloseNotifier([&is_closed]() { is_closed = true; });
  EXPECT_TRUE(server_socket.Close());
  EXPECT_TRUE(is_closed);
}

}  // namespace
}  // namespace nearby::windows
