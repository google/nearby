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

#ifndef CORE_INTERNAL_WIFI_LAN_ENDPOINT_CHANNEL_V2_H_
#define CORE_INTERNAL_WIFI_LAN_ENDPOINT_CHANNEL_V2_H_

#include "core/internal/base_endpoint_channel.h"
#include "platform/public/wifi_lan_v2.h"

namespace location {
namespace nearby {
namespace connections {

class WifiLanEndpointChannelV2 final : public BaseEndpointChannel {
 public:
  // Creates both outgoing and incoming WifiLan channels.
  WifiLanEndpointChannelV2(const std::string& channel_name,
                           WifiLanSocketV2 socket);

  proto::connections::Medium GetMedium() const override;

 private:
  void CloseImpl() override;

  WifiLanSocketV2 socket_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_WIFI_LAN_ENDPOINT_CHANNEL_V2_H_
