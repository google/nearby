
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
#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_AWDL_ENDPOINT_CHANNEL_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_AWDL_ENDPOINT_CHANNEL_H_


#include <string>
#include "connections/implementation/base_endpoint_channel.h"
#include "internal/platform/awdl.h"

namespace nearby {
namespace connections {

class AwdlEndpointChannel final : public BaseEndpointChannel {
 public:
  // Creates both outgoing and incoming Awdl channels.
  AwdlEndpointChannel(const std::string& service_id,
                            const std::string& channel_name,
                            AwdlSocket socket);

  location::nearby::proto::connections::Medium GetMedium() const override;
  bool EnableMultiplexSocket() override;

 private:
  void CloseImpl() override;

  AwdlSocket socket_;
};

}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_AWDL_ENDPOINT_CHANNEL_H_
