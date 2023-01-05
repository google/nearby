// Copyright 2022 Google LLC
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


#ifndef CORE_INTERNAL_WIFI_HOTSPOT_ENDPOINT_CHANNEL_H_
#define CORE_INTERNAL_WIFI_HOTSPOT_ENDPOINT_CHANNEL_H_

#include <string>

#include "connections/implementation/base_endpoint_channel.h"
#include "internal/platform/wifi_hotspot.h"

namespace nearby {
namespace connections {

class WifiHotspotEndpointChannel final : public BaseEndpointChannel {
 public:
  // Creates both outgoing and incoming WifiHotspot channels.
  WifiHotspotEndpointChannel(const std::string& service_id,
                             const std::string& channel_name,
                             WifiHotspotSocket socket);
  // Not copyable or movable
  WifiHotspotEndpointChannel(const WifiHotspotEndpointChannel&) = delete;
  WifiHotspotEndpointChannel& operator=(const WifiHotspotEndpointChannel&) =
      delete;
  WifiHotspotEndpointChannel(WifiHotspotEndpointChannel&&) = delete;
  WifiHotspotEndpointChannel& operator=(WifiHotspotEndpointChannel&&) = delete;

  location::nearby::proto::connections::Medium GetMedium() const override;

 private:
  void CloseImpl() override;

  WifiHotspotSocket socket_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_WIFI_HOTSPOT_ENDPOINT_CHANNEL_H_
