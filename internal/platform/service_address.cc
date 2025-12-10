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

#include <string>

#include "connections/implementation/proto/offline_wire_formats.pb.h"

namespace nearby {

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
  // Address must be either 4 or 16 bytes and port must be set.
  if ((proto.ip_address().size() != 16 && proto.ip_address().size() != 4) ||
      proto.port() == 0) {
    return false;
  }
  service_address.address = {proto.ip_address().begin(),
                             proto.ip_address().end()};
  service_address.port = proto.port();
  return true;
}

}  // namespace nearby
