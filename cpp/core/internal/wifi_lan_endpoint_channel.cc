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

#include "core/internal/wifi_lan_endpoint_channel.h"

#include <string>

#include "platform/public/logging.h"
#include "platform/public/wifi_lan.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

OutputStream* GetOutputStreamOrNull(WifiLanSocket& socket) {
  if (socket.GetRemoteWifiLanService().IsValid())
    return &socket.GetOutputStream();
  return nullptr;
}

InputStream* GetInputStreamOrNull(WifiLanSocket& socket) {
  if (socket.GetRemoteWifiLanService().IsValid())
    return &socket.GetInputStream();
  return nullptr;
}

}  // namespace

WifiLanEndpointChannel::WifiLanEndpointChannel(const std::string& channel_name,
                                               WifiLanSocket socket)
    : BaseEndpointChannel(channel_name, GetInputStreamOrNull(socket),
                          GetOutputStreamOrNull(socket)),
      wifi_lan_socket_(std::move(socket)) {}

proto::connections::Medium WifiLanEndpointChannel::GetMedium() const {
  return proto::connections::Medium::WIFI_LAN;
}

void WifiLanEndpointChannel::CloseImpl() {
  auto status = wifi_lan_socket_.Close();
  if (!status.Ok()) {
    NEARBY_LOG(INFO,
               "Failed to close underlying socket for WifiLanEndpointChannel "
               "%s: exception=%d",
               GetName().c_str(), status.value);
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
