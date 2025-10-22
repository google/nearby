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

#include <winsock2.h>
#include <ws2ipdef.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "absl/types/span.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

SocketAddress::SocketAddress(const sockaddr_in& address, bool dual_stack)
    : dual_stack_(dual_stack) {
  std::memcpy(&address_, &address, sizeof(sockaddr_in));
  address_.ss_family = AF_INET;
  if (dual_stack_) {
    ToMappedIPv6();
  }
}

SocketAddress::SocketAddress(const sockaddr_in6& address) : dual_stack_(true) {
  std::memcpy(&address_, &address, sizeof(sockaddr_in6));
  address_.ss_family = AF_INET6;
}

SocketAddress::SocketAddress(const sockaddr_storage& address)
    : dual_stack_(true) {
  DCHECK(address.ss_family == AF_INET || address.ss_family == AF_INET6);
  std::memcpy(&address_, &address, sizeof(sockaddr_storage));
  ToMappedIPv6();
}

void SocketAddress::ToMappedIPv6() {
  if (address_.ss_family == AF_INET6) {
    return;
  }
  if (address_.ss_family != AF_INET) {
    LOG(ERROR) << "Unknown socket family: " << address_.ss_family;
    return;
  }
  sockaddr_in* v4_address = reinterpret_cast<sockaddr_in*>(&address_);
  in_addr orig_address;
  std::memcpy(&orig_address, &v4_address->sin_addr, sizeof(in_addr));
  int orig_port = v4_address->sin_port;
  address_.ss_family = AF_INET6;
  sockaddr_in6* v6_address = reinterpret_cast<sockaddr_in6*>(&address_);
  v6_address->sin6_port = orig_port;
  v6_address->sin6_flowinfo = 0;
  v6_address->sin6_scope_id = 0;
  v6_address->sin6_addr.u.Word[0] = 0;
  v6_address->sin6_addr.u.Word[1] = 0;
  v6_address->sin6_addr.u.Word[2] = 0;
  v6_address->sin6_addr.u.Word[3] = 0;
  v6_address->sin6_addr.u.Word[4] = 0;
  v6_address->sin6_addr.u.Word[5] = 0xffff;
  v6_address->sin6_addr.u.Word[6] = orig_address.S_un.S_un_w.s_w1;
  v6_address->sin6_addr.u.Word[7] = orig_address.S_un.S_un_w.s_w2;
}

bool SocketAddress::FromString(SocketAddress& address,
                               std::string address_string, int port) {
  if (address_string.empty()) {
    if (address.dual_stack_) {
      address.address_.ss_family = AF_INET6;
      sockaddr_in6* v6_address =
          reinterpret_cast<sockaddr_in6*>(&address.address_);
      v6_address->sin6_port = 0;
      v6_address->sin6_flowinfo = 0;
      v6_address->sin6_scope_id = 0;
      v6_address->sin6_addr = in6addr_any;
      address.set_port(port);
      return true;
    }
    address.address_.ss_family = AF_INET;
    sockaddr_in* v4_address = reinterpret_cast<sockaddr_in*>(&address.address_);
    v4_address->sin_port = 0;
    v4_address->sin_addr.s_addr = INADDR_ANY;
    address.set_port(port);
    return true;
  }
  // Try v4 address first.
  address.address_.ss_family = AF_INET;
  int sock_address_size = sizeof(sockaddr_storage);
  if (WSAStringToAddressA(address_string.data(), AF_INET,
                          /*lpProtocolInfo=*/nullptr,
                          reinterpret_cast<sockaddr*>(&address.address_),
                          &sock_address_size) == 0) {
    if (address.dual_stack_) {
      address.ToMappedIPv6();
    }
    address.set_port(port);
    return true;
  }
  // Try v6 address if v4 address is not supported.
  if (!address.dual_stack_) {
    return false;
  }
  address.address_.ss_family = AF_INET6;
  sock_address_size = sizeof(sockaddr_storage);
  if (WSAStringToAddressA(const_cast<char*>(address_string.data()), AF_INET6,
                          /*lpProtocolInfo=*/nullptr,
                          reinterpret_cast<sockaddr*>(&address.address_),
                          &sock_address_size) == 0) {
    address.set_port(port);
    return true;
  }
  return false;
}

bool SocketAddress::FromBytes(SocketAddress& address,
                              absl::Span<const char> address_bytes, int port) {
  if (address_bytes.size() != 4 &&
      !(address.dual_stack_ && address_bytes.size() == 16)) {
    // Invalid address bytes size.
    return false;
  }
  if (address_bytes.size() == 4) {
    address.address_.ss_family = AF_INET;
    sockaddr_in* v4_address = reinterpret_cast<sockaddr_in*>(&address.address_);
    std::memcpy(&v4_address->sin_addr, address_bytes.data(), sizeof(in_addr));
    address.set_port(port);
    if (address.dual_stack_) {
      address.ToMappedIPv6();
    }
    return true;
  }
  address.address_.ss_family = AF_INET6;
  sockaddr_in6* v6_address =
      reinterpret_cast<sockaddr_in6*>(&address.address_);
  std::memcpy(&v6_address->sin6_addr, address_bytes.data(), sizeof(in6_addr));
  address.set_port(port);
  return true;
}

int SocketAddress::port() const {
  DCHECK(address_.ss_family == AF_INET || address_.ss_family == AF_INET6);
  if (address_.ss_family == AF_INET) {
    const sockaddr_in* v4_address =
        reinterpret_cast<const sockaddr_in*>(&address_);
    return ntohs(v4_address->sin_port);
  }
  if (address_.ss_family == AF_INET6) {
    const sockaddr_in6* v6_address =
        reinterpret_cast<const sockaddr_in6*>(&address_);
    return ntohs(v6_address->sin6_port);
  }
  LOG(ERROR) << "Unknown socket family: " << address_.ss_family;
  return 0;
}

bool SocketAddress::set_port(int port) {
  DCHECK(address_.ss_family == AF_INET || address_.ss_family == AF_INET6);
  if (port < 0 || port > 65535) {
    LOG(ERROR) << "Invalid port: " << port;
    return false;
  }
  if (address_.ss_family == AF_INET) {
    sockaddr_in* v4_address = reinterpret_cast<sockaddr_in*>(&address_);
    v4_address->sin_port = htons(port);
    return true;
  }
  if (address_.ss_family == AF_INET6) {
    sockaddr_in6* v6_address = reinterpret_cast<sockaddr_in6*>(&address_);
    v6_address->sin6_port = htons(port);
    return true;
  }
  LOG(ERROR) << "Unknown socket family: " << address_.ss_family;
  return false;
}

std::string SocketAddress::ToString() const {
  std::string address_string;
  DWORD size = INET6_ADDRSTRLEN;  // Max IP address length.
  address_string.resize(size);
  if (WSAAddressToStringA(
          const_cast<sockaddr*>(reinterpret_cast<const sockaddr*>(&address_)),
          sizeof(sockaddr_storage),
          /*lpProtocolInfo=*/nullptr, address_string.data(), &size) != 0) {
    LOG(ERROR) << __func__
               << ": Cannot convert address to string: " << WSAGetLastError();
    return "";
  }
  // size includes the null terminator.
  address_string.resize(size - 1);
  return address_string;
}

bool SocketAddress::IsV6LinkLocal() const {
  if (address_.ss_family != AF_INET6) {
    return false;
  }
  const sockaddr_in6* v6_address =
      reinterpret_cast<const sockaddr_in6*>(&address_);
  return IN6_IS_ADDR_LINKLOCAL(&v6_address->sin6_addr);
}

bool SocketAddress::SetScopeId(uint32_t scope_id) {
  if (address_.ss_family != AF_INET6) {
    return false;
  }
  sockaddr_in6* v6_address = reinterpret_cast<sockaddr_in6*>(&address_);
  v6_address->sin6_scope_id = scope_id;
  return true;
}

}  // namespace nearby::windows
