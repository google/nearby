// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_CONNECTIONS_AUTHENTICATION_TRANSPORT_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_CONNECTIONS_AUTHENTICATION_TRANSPORT_H_

#include <string>

#include "absl/strings/string_view.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/interop/authentication_transport.h"

namespace nearby {
namespace connections {

// The messages passed through this channel should not be UTF-8 decoded, as they
// will consist of protobuf-serialized data, and std::string is used as a
// container for bytes.
class ConnectionsAuthenticationTransport
    : public nearby::AuthenticationTransport {
 public:
  explicit ConnectionsAuthenticationTransport(const EndpointChannel& channel);
  void WriteMessage(absl::string_view message) const override;
  std::string ReadMessage() const override;

 private:
  EndpointChannel* channel_;
};

}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_CONNECTIONS_AUTHENTICATION_TRANSPORT_H_
