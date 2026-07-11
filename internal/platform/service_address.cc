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

#include "internal/platform/service_address.h"

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"

namespace nearby {
namespace {
constexpr absl::string_view kIpv6LoopbackAddress(
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1", 16);
}  // namespace

void ServiceAddressToProto(
    const ServiceAddress& service_address,
    location::nearby::connections::ServiceAddress& proto) {
  proto.set_ip_address(std::string(service_address.address.begin(),
                                   service_address.address.end()));
  proto.set_port(service_address.port);
}

bool ServiceAddressFromProto(
    const location::nearby::connections::ServiceAddress& proto,
    ServiceAddress& service_address) {
  // Address must be either 4 or 16 bytes and port must be valid (1 to 65535).
  if ((proto.ip_address().size() != 16 && proto.ip_address().size() != 4) ||
      proto.port() <= 0 || proto.port() > 65535) {
    return false;
  }
  service_address.address = {proto.ip_address().begin(),
                             proto.ip_address().end()};
  service_address.port = proto.port();
  return true;
}

bool ServiceAddress::IsLoopbackAddress() const {
  if (address.size() == 4) {
    // IPv4 loopback: 127.0.0.0/8
    return address[0] == 127;
  } else if (address.size() == 16) {
    // IPv6 loopback: ::1
    return absl::string_view(address.data(), address.size()) ==
           kIpv6LoopbackAddress;
  }
  return false;
}

bool ServiceAddress::IsLinkLocalAddress() const {
  if (address.size() == 4) {
    // IPv4 link-local: 169.254.0.0/16
    uint8_t b0 = static_cast<uint8_t>(address[0]);
    uint8_t b1 = static_cast<uint8_t>(address[1]);
    return b0 == 169 && b1 == 254;
  } else if (address.size() == 16) {
    // IPv6 link-local: fe80::/10
    uint8_t b0 = static_cast<uint8_t>(address[0]);
    uint8_t b1 = static_cast<uint8_t>(address[1]);
    return b0 == 0xfe && (b1 & 0xc0) == 0x80;
  }
  return false;
}

}  // namespace nearby
