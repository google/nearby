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

#include "connections/implementation/connections_authentication_transport.h"

#include <string>

#include "absl/strings/string_view.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {

ConnectionsAuthenticationTransport::ConnectionsAuthenticationTransport(
    const EndpointChannel& channel) {
  channel_ = const_cast<EndpointChannel*>(&channel);
}

void ConnectionsAuthenticationTransport::WriteMessage(
    absl::string_view message) const {
  // channel_ should never be null.
  CHECK(channel_ != nullptr);
  channel_->Write(ByteArray(message.data(), message.size()));
}

std::string ConnectionsAuthenticationTransport::ReadMessage() const {
  // channel_ should never be null.
  CHECK(channel_ != nullptr);
  auto response = channel_->Read();
  if (response.ok()) {
    return response.result().string_data();
  }
  NEARBY_LOGS(WARNING) << "ConnectionsAuthenticationTransport: read failed "
                          "with exception/result: "
                       << response.exception();
  return "";
}

}  // namespace connections
}  // namespace nearby
