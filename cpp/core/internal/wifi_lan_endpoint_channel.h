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

#ifndef CORE_INTERNAL_WIFI_LAN_ENDPOINT_CHANNEL_H_
#define CORE_INTERNAL_WIFI_LAN_ENDPOINT_CHANNEL_H_

#include "core/internal/base_endpoint_channel.h"
#include "core/internal/medium_manager.h"
#include "platform/api/platform.h"
#include "platform/api/wifi_lan.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "proto/connections_enums.pb.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {
namespace connections {

class WifiLanEndpointChannel : public BaseEndpointChannel {
 public:
  using Platform = platform::ImplementationPlatform;

  static Ptr<WifiLanEndpointChannel> CreateOutgoing(
      Ptr<MediumManager<Platform>> medium_manager,
      absl::string_view channel_name, Ptr<WifiLanSocket> wifi_lan_socket);
  static Ptr<WifiLanEndpointChannel> CreateIncoming(
      Ptr<MediumManager<Platform>> medium_manager,
      absl::string_view channel_name, Ptr<WifiLanSocket> wifi_lan_socket);

  ~WifiLanEndpointChannel() override;

  proto::connections::Medium getMedium() override;

 protected:
  void closeImpl() override;

 private:
  WifiLanEndpointChannel(absl::string_view channel_name,
                         Ptr<WifiLanSocket> wifi_lan_socket);

  ScopedPtr<Ptr<WifiLanSocket> > wifi_lan_socket_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_WIFI_LAN_ENDPOINT_CHANNEL_H_
