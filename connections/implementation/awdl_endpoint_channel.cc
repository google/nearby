// Copyright 2020 Google LLC
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

#include "connections/implementation/awdl_endpoint_channel.h"

#include <string>

#include "connections/implementation/base_endpoint_channel.h"
#include "internal/platform/logging.h"
#include "internal/platform/awdl.h"

namespace nearby {
namespace connections {
AwdlEndpointChannel::AwdlEndpointChannel(const std::string& service_id,
                                               const std::string& channel_name,
                                               AwdlSocket socket)
    : BaseEndpointChannel(service_id, channel_name, &socket.GetInputStream(),
                          &socket.GetOutputStream()),
      socket_(std::move(socket)) {}

location::nearby::proto::connections::Medium AwdlEndpointChannel::GetMedium()
    const {
  return location::nearby::proto::connections::Medium::AWDL;
}

void AwdlEndpointChannel::CloseImpl() {
  auto status = socket_.Close();
  if (!status.Ok()) {
    NEARBY_LOGS(INFO)
        << "Failed to close underlying socket for AwdlEndpointChannel "
        << GetName() << " : exception = " << status.value;
  }
}

bool AwdlEndpointChannel::EnableMultiplexSocket() {
  NEARBY_LOGS(INFO) << "AwdlEndpointChannel MultiplexSocket will be "
                       "enabled if the Awdl MultiplexSocket is valid";
  socket_.EnableMultiplexSocket();
  return true;
}

}  // namespace connections
}  // namespace nearby
