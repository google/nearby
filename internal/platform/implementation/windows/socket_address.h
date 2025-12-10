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
// IPv6 support needs to be enabled by setting `dual_stack` to true.
class SocketAddress {
 public:
  explicit SocketAddress(bool dual_stack = false) : dual_stack_(dual_stack) {
    std::memset(&address_, 0, sizeof(address_));
    if (dual_stack_) {
      address_.ss_family = AF_INET6;
    } else {
      address_.ss_family = AF_INET;
    }
  }
  explicit SocketAddress(const sockaddr_in& address, bool dual_stack = false);
  explicit SocketAddress(const sockaddr_in6& address);
  // This constructor assumes dual_stack is enabled, and will convert IPv4
  // addresses to mapped IPv6 addresses.
  explicit SocketAddress(const sockaddr_storage& address);

  ~SocketAddress() = default;

  // The `dual_stack` state of `address` determines whether the address is
  // can be parsed as IPv6.
  // If dual_stack is enabled, an IPv4 string will be returned as a mapped IPv6
  // address (e.g. [::ffff:192.0.2.1]).
  // Use empty `address_string` to create and unspecified address ie. ADDR_ANY.
  // `port` is in host byte order.
  static bool FromString(SocketAddress& address, std::string address_string,
                         int port = 0);

  // The `dual_stack` state of `address` determines whether the address is
  // can be parsed as IPv6.
  // `addresss_bytes` is 4 bytes if dual_stack is disabled, and 4 or 16 bytes
  // for dual_stack.  The address must be in network byte order.
  // `port` is in host byte order.
  static bool FromBytes(SocketAddress& address,
                        absl::Span<const char> address_bytes, int port = 0);

  static bool FromServiceAddress(SocketAddress& address,
                                 const ServiceAddress& service_address);

  // Returns true if dual stack support has been enabled.
  bool dual_stack() const { return dual_stack_; }

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

  // Returns a pointer to the internal sockaddr_storage.
  // This can be used to modify the address directly.  However, the dual_stack
  // state is not honored, ie. it will not convert IPv4 addresses to mapped IPv6
  // addresses.
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
  // If `address_` is AF_INET, then rewrite into mapped ipv6 address, e.g.
  // [::ffff:192.0.2.1].
  void ToMappedIPv6();

  const bool dual_stack_ = false;
  sockaddr_storage address_;
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_SOCKET_ADDRESS_H_
