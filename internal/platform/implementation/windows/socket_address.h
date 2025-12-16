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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_SOCKET_ADDRESS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_SOCKET_ADDRESS_H_

#include <winsock2.h>
#include <ws2ipdef.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "absl/types/span.h"
#include "internal/platform/service_address.h"

namespace nearby::windows {

// A helper class that simplifies handling of both IPv4 and IPv6 addresses.
class SocketAddress {
 public:
  // Default constructor creates an unspecified IPv6 address ie. IN6ADDR_ANY
  // and port 0.
  SocketAddress() {
    std::memset(&address_, 0, sizeof(address_));
    address_.ss_family = AF_INET6;
    set_port(0);
  }

  explicit SocketAddress(const sockaddr_in& address);
  explicit SocketAddress(const sockaddr_in6& address);
  explicit SocketAddress(const sockaddr_storage& address);

  ~SocketAddress() = default;

  // Empty `address_string`creates an unspecified IPv6 address ie. IN6ADDR_ANY.
  // `port` is in host byte order.
  static bool FromString(SocketAddress& address, std::string address_string,
                         int port = 0);

  // `addresss_bytes` is 4 or 16 bytes for IPv4 and IPv6 respectively.
  //  The address must be in network byte order.
  // `port` is in host byte order.
  static bool FromBytes(SocketAddress& address,
                        absl::Span<const char> address_bytes, int port = 0);

  static bool FromServiceAddress(SocketAddress& address,
                                 const ServiceAddress& service_address);

  // Returns either AF_INET or AF_INET6.
  int family() const { return address_.ss_family; }

  // `Returns port in host byte order.
  int port() const;
  // `port` is in host byte order.
  bool set_port(int port);

  std::string ToString() const;

  // Returns true if the address is a link local IPv6 address, ie. FE80::XXXX.
  // Returns false if the address is not IPv6 or is not link local.
  bool IsV6LinkLocal() const;

  // Sets the scope id of the address.
  // Returns false if the address is not IPv6.
  bool SetScopeId(uint32_t scope_id);

  // If `address_` is AF_INET, then rewrite into mapped ipv6 address, e.g.
  // [::ffff:192.0.2.1].
  SocketAddress ToMappedIPv6() const;

  // Returns a pointer to the internal sockaddr_storage.
  // This can be used to modify the address directly.
  sockaddr* address() {
    return reinterpret_cast<sockaddr*>(&address_);
  }

  // An overload to return a non-const sockaddr pointer from a const
  // SocketAddress.  This is a convenience for calling legacy C APIs.
  sockaddr* address() const {
    return const_cast<sockaddr*>(reinterpret_cast<const sockaddr*>(&address_));
  }

  const sockaddr_in* ipv4_address() const {
    return reinterpret_cast<const sockaddr_in*>(&address_);
  }
  const sockaddr_in6* ipv6_address() const {
    return reinterpret_cast<const sockaddr_in6*>(&address_);
  }

 private:
  sockaddr_storage address_;
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_SOCKET_ADDRESS_H_
