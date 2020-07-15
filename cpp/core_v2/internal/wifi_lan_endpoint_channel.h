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

#ifndef CORE_V2_INTERNAL_WIFI_LAN_ENDPOINT_CHANNEL_H_
#define CORE_V2_INTERNAL_WIFI_LAN_ENDPOINT_CHANNEL_H_

#include "core_v2/internal/base_endpoint_channel.h"
#include "platform_v2/public/wifi_lan.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

class WifiLanEndpointChannel final : public BaseEndpointChannel {
 public:
  // Creates both outgoing and incoming WifiLan channels.
  WifiLanEndpointChannel(const std::string& channel_name,
                         WifiLanSocket socket);

  proto::connections::Medium GetMedium() const override;

 private:
  void CloseImpl() override;

  WifiLanSocket wifi_lan_socket_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_WIFI_LAN_ENDPOINT_CHANNEL_H_
