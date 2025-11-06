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

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/windows/socket_address.h"

namespace nearby::windows {
namespace {

SOCKET CreateSocket(const SocketAddress& address) {
  SOCKET socket_handle = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
  DWORD v6_only = 0;
  EXPECT_NE(
      setsockopt(socket_handle, IPPROTO_IPV6, IPV6_V6ONLY,
                 reinterpret_cast<const char*>(&v6_only), sizeof(v6_only)),
      SOCKET_ERROR);
  EXPECT_NE(
      bind(socket_handle, address.address(), sizeof(sockaddr_storage)),
      SOCKET_ERROR);
  return socket_handle;
}

SocketAddress GetLocalAddress(SOCKET socket_handle) {
  SocketAddress local_address(/*dual_stack=*/true);
  int address_length = sizeof(sockaddr_storage);
  EXPECT_NE(
      getsockname(socket_handle, local_address.address(), &address_length),
      SOCKET_ERROR);
  return local_address;
}

TEST(NearbyClientSocketTest, ConnectWithBoundSocketFails) {
  SOCKET socket_handle = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
  NearbyClientSocket client_socket(socket_handle);
  SocketAddress server_address(/*dual_stack=*/true);
  SocketAddress::FromString(server_address, "::1", 8080);

  EXPECT_FALSE(client_socket.Connect(server_address, absl::InfiniteDuration()));

  closesocket(socket_handle);
}

TEST(NearbyClientSocketTest, Connect) {
  SocketAddress local_address(/*dual_stack=*/true);
  SocketAddress::FromString(local_address, "", 0);
  SOCKET socket_handle = CreateSocket(local_address);
  SocketAddress bound_address = GetLocalAddress(socket_handle);
  EXPECT_NE(listen(socket_handle, SOMAXCONN), SOCKET_ERROR);
  NearbyClientSocket client_socket;
  SocketAddress server_address(/*dual_stack=*/true);
  SocketAddress::FromString(server_address, "::1", bound_address.port());

  EXPECT_TRUE(client_socket.Connect(server_address, absl::InfiniteDuration()));

  closesocket(socket_handle);
}

TEST(NearbyClientSocketTest, CloseNotOpened) {
  NearbyClientSocket client_socket;
  EXPECT_TRUE(client_socket.Close().Ok());
}

TEST(NearbyClientSocketTest, Read) {
  SocketAddress local_address(/*dual_stack=*/true);
  SocketAddress::FromString(local_address, "", 0);
  SOCKET socket_handle = CreateSocket(local_address);
  SocketAddress bound_address = GetLocalAddress(socket_handle);
  EXPECT_NE(listen(socket_handle, SOMAXCONN), SOCKET_ERROR);
  NearbyClientSocket client_socket;
  SocketAddress server_address(/*dual_stack=*/true);
  SocketAddress::FromString(server_address, "::1", bound_address.port());
  EXPECT_TRUE(client_socket.Connect(server_address, absl::InfiniteDuration()));
  SocketAddress peer_address;
  int peer_address_length = sizeof(sockaddr_storage);
  SOCKET accept_socket = accept(socket_handle, peer_address.address(),
                                /*addrlen=*/&peer_address_length);
  EXPECT_NE(accept_socket, INVALID_SOCKET);
  EXPECT_NE(send(accept_socket, "hello", 5, 0), SOCKET_ERROR);

  EXPECT_EQ(client_socket.Read(5).result(), ByteArray("hello"));

  closesocket(socket_handle);
}

TEST(NearbyClientSocketTest, Skip) {
  SocketAddress local_address(/*dual_stack=*/true);
  SocketAddress::FromString(local_address, "", 0);
  SOCKET socket_handle = CreateSocket(local_address);
  SocketAddress bound_address = GetLocalAddress(socket_handle);
  EXPECT_NE(listen(socket_handle, SOMAXCONN), SOCKET_ERROR);
  NearbyClientSocket client_socket;
  SocketAddress server_address(/*dual_stack=*/true);
  SocketAddress::FromString(server_address, "::1", bound_address.port());
  EXPECT_TRUE(client_socket.Connect(server_address, absl::InfiniteDuration()));
  SocketAddress peer_address;
  int peer_address_length = sizeof(sockaddr_storage);
  SOCKET accept_socket = accept(socket_handle, peer_address.address(),
                                /*addrlen=*/&peer_address_length);
  EXPECT_NE(accept_socket, INVALID_SOCKET);
  EXPECT_NE(send(accept_socket, "hello there", 11, 0), SOCKET_ERROR);

  EXPECT_EQ(client_socket.Skip(6).result(), 6);

  EXPECT_EQ(client_socket.Read(5).result(), ByteArray("there"));
  closesocket(socket_handle);
}

TEST(NearbyClientSocketTest, Write) {
  SocketAddress local_address(/*dual_stack=*/true);
  SocketAddress::FromString(local_address, "", 0);
  SOCKET socket_handle = CreateSocket(local_address);
  SocketAddress bound_address = GetLocalAddress(socket_handle);
  EXPECT_NE(listen(socket_handle, SOMAXCONN), SOCKET_ERROR);
  NearbyClientSocket client_socket;
  SocketAddress server_address(/*dual_stack=*/true);
  SocketAddress::FromString(server_address, "::1", bound_address.port());
  EXPECT_TRUE(client_socket.Connect(server_address, absl::InfiniteDuration()));
  SocketAddress peer_address;
  int peer_address_length = sizeof(sockaddr_storage);
  SOCKET accept_socket = accept(socket_handle, peer_address.address(),
                                /*addrlen=*/&peer_address_length);
  EXPECT_NE(accept_socket, INVALID_SOCKET);

  EXPECT_TRUE(client_socket.Write(ByteArray("hello")).Ok());
  std::string buffer;
  buffer.resize(5);
  EXPECT_EQ(recv(accept_socket, buffer.data(), 5, 0), 5);
  EXPECT_EQ(buffer, "hello");

  closesocket(socket_handle);
}

}  // namespace
}  // namespace nearby::windows
