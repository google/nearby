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

#include "internal/platform/implementation/windows/socket_address.h"

#include "gtest/gtest.h"
#include "internal/platform/service_address.h"

namespace nearby::windows {
namespace {

TEST(SocketAddressTest, CreateFromSockAddrIn) {
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_port = htons(8080);
  address.sin_addr.s_addr = inet_addr("192.168.1.1");
  SocketAddress socket_address(address);
  EXPECT_EQ(socket_address.family(), AF_INET);
  EXPECT_EQ(socket_address.ToString(), "192.168.1.1:8080");
  EXPECT_EQ(socket_address.port(), 8080);
}

TEST(SocketAddressTest, CreateFromSockAddrIn6) {
  sockaddr_in6 address;
  address.sin6_family = AF_INET6;
  address.sin6_port = htons(8080);
  address.sin6_flowinfo = 0;
  address.sin6_scope_id = 0;
  address.sin6_addr.u.Word[0] = 0xFDDF;
  address.sin6_addr.u.Word[1] = 0x253D;
  address.sin6_addr.u.Word[2] = 0x0BA3;
  address.sin6_addr.u.Word[3] = 0x51A2;
  address.sin6_addr.u.Word[4] = 0x51ED;
  address.sin6_addr.u.Word[5] = 0xEE8F;
  address.sin6_addr.u.Word[6] = 0x8F1C;
  address.sin6_addr.u.Word[7] = 0xB30C;
  SocketAddress socket_address(address);
  EXPECT_EQ(socket_address.family(), AF_INET6);
  EXPECT_EQ(socket_address.ToString(),
            "[dffd:3d25:a30b:a251:ed51:8fee:1c8f:cb3]:8080");
  EXPECT_EQ(socket_address.port(), 8080);
}

TEST(SocketAddressTest, FromStringIPv4) {
  SocketAddress address;
  EXPECT_TRUE(SocketAddress::FromString(address, "192.168.1.1", 8080));
  EXPECT_EQ(address.family(), AF_INET);
  EXPECT_EQ(address.ToString(), "192.168.1.1:8080");
  EXPECT_EQ(address.port(), 8080);
}

TEST(SocketAddressTest, FromStringIPv4NoPort) {
  SocketAddress address;
  EXPECT_TRUE(SocketAddress::FromString(address, "192.168.1.1"));
  EXPECT_EQ(address.family(), AF_INET);
  EXPECT_EQ(address.ToString(), "192.168.1.1");
  EXPECT_EQ(address.port(), 0);
}

TEST(SocketAddressTest, FromStringIPv6) {
  SocketAddress address;
  EXPECT_TRUE(SocketAddress::FromString(address, "2001:db8::1", 8080));
  EXPECT_EQ(address.family(), AF_INET6);
  EXPECT_EQ(address.ToString(), "[2001:db8::1]:8080");
  EXPECT_EQ(address.port(), 8080);
}

TEST(SocketAddressTest, FromStringIPv6NoPort) {
  SocketAddress address;
  EXPECT_TRUE(SocketAddress::FromString(address, "2001:db8::1"));
  EXPECT_EQ(address.family(), AF_INET6);
  EXPECT_EQ(address.ToString(), "2001:db8::1");
  EXPECT_EQ(address.port(), 0);
}

TEST(SocketAddressTest, FromStringAnyAddress) {
  SocketAddress address;
  EXPECT_TRUE(SocketAddress::FromString(address, ""));
  EXPECT_EQ(address.family(), AF_INET6);
  EXPECT_EQ(address.ToString(), "::");
}

TEST(SocketAddressTest, FromStringMappedIPv4) {
  SocketAddress address;
  EXPECT_TRUE(SocketAddress::FromString(address, "::ffff:192.168.1.1", 8080));
  EXPECT_EQ(address.family(), AF_INET6);
  EXPECT_EQ(address.port(), 8080);
  EXPECT_EQ(address.ToString(), "[::ffff:192.168.1.1]:8080");
}

TEST(SocketAddressTest, FromStringInvalid) {
  SocketAddress address;
  EXPECT_FALSE(SocketAddress::FromString(address, "invalid", 8080));
}

TEST(SocketAddressTest, ToMappedIPv6) {
  SocketAddress v4_address;
  EXPECT_TRUE(SocketAddress::FromString(v4_address, "192.168.1.1", 8080));
  SocketAddress v6_address = v4_address.ToMappedIPv6();
  EXPECT_EQ(v6_address.family(), AF_INET6);
  EXPECT_EQ(v6_address.ToString(), "[::ffff:192.168.1.1]:8080");
  EXPECT_EQ(v6_address.port(), 8080);
}

TEST(SocketAddressTest, SetPort) {
  SocketAddress address;
  EXPECT_TRUE(SocketAddress::FromString(address, "127.0.0.1", 8080));
  EXPECT_EQ(address.port(), 8080);
  EXPECT_TRUE(address.set_port(9090));
  EXPECT_EQ(address.port(), 9090);
}

TEST(SocketAddressTest, SetInvalidPort) {
  SocketAddress address;
  EXPECT_TRUE(SocketAddress::FromString(address, "127.0.0.1", 8080));
  EXPECT_FALSE(address.set_port(-1));
  EXPECT_FALSE(address.set_port(65536));
  EXPECT_EQ(address.port(), 8080);
}

TEST(SocketAddressTest, FromBytesIPv4) {
  SocketAddress address;
  char bytes[4] = {192, 168, 1, 1};
  EXPECT_TRUE(SocketAddress::FromBytes(address, bytes, 8080));
  EXPECT_EQ(address.family(), AF_INET);
  EXPECT_EQ(address.ToString(), "192.168.1.1:8080");
  EXPECT_EQ(address.port(), 8080);
}

TEST(SocketAddressTest, FromBytesIPv6DualStack) {
  SocketAddress address;
  char bytes[16] = {0xfe, 0x80, 0,    0,    0,    0,    0,    0,
                    0x4d, 0xb2, 0xb3, 0x5c, 0x22, 0x03, 0x98, 0xa1};
  EXPECT_TRUE(SocketAddress::FromBytes(address, bytes, 8080));
  EXPECT_EQ(address.family(), AF_INET6);
  EXPECT_EQ(address.ToString(), "[fe80::4db2:b35c:2203:98a1]:8080");
  EXPECT_EQ(address.port(), 8080);
}

TEST(SocketAddressTest, IPv6LinkLocalSuccess) {
  SocketAddress address;
  char bytes[16] = {0xfe, 0x80, 0,    0,    0,    0,    0,    0,
                    0x4d, 0xb2, 0xb3, 0x5c, 0x22, 0x03, 0x98, 0xa1};
  EXPECT_TRUE(SocketAddress::FromBytes(address, bytes, 8080));
  EXPECT_TRUE(address.IsV6LinkLocal());
}

TEST(SocketAddressTest, IPv6LinkLocalFail) {
  SocketAddress address;
  char bytes[16] = {0x20, 0x01, 0x0d, 0xb8,   0,    0,    0,    0,
                    0x4d, 0xb2, 0xb3, 0x5c, 0x22, 0x03, 0x98, 0xa1};
  EXPECT_TRUE(SocketAddress::FromBytes(address, bytes, 8080));
  EXPECT_FALSE(address.IsV6LinkLocal());
}

TEST(SocketAddressTest, FromServiceAddressIPv4) {
  SocketAddress address;
  ServiceAddress service_address = {
      .address = {192, 168, 1, 1},
      .port = 8080,
  };
  EXPECT_TRUE(SocketAddress::FromServiceAddress(address, service_address));
  EXPECT_EQ(address.family(), AF_INET);
  EXPECT_EQ(address.ToString(), "192.168.1.1:8080");
  EXPECT_EQ(address.port(), 8080);
}

TEST(SocketAddressTest, FromServiceAddressIPv6) {
  SocketAddress address;
  ServiceAddress service_address = {
      .address = {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x4d, 0xb2, 0xb3, 0x5c, 0x22,
                  0x03, 0x98, 0xa1},
      .port = 8080,
  };
  EXPECT_TRUE(SocketAddress::FromServiceAddress(address, service_address));
  EXPECT_EQ(address.family(), AF_INET6);
  EXPECT_EQ(address.ToString(), "[fe80::4db2:b35c:2203:98a1]:8080");
  EXPECT_EQ(address.port(), 8080);
}

TEST(SocketAddressTest, FromServiceAddressInvalidAddress) {
  SocketAddress address;
  ServiceAddress service_address = {
      .address = {192, 168, 1},
      .port = 8080,
  };
  EXPECT_FALSE(SocketAddress::FromServiceAddress(address, service_address));
}

}  // namespace
}  // namespace nearby::windows
