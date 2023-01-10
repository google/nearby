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

#include "connections/cpp/implementation/webrtc_endpoint_channel.h"

#include <string>

namespace nearby {
namespace connections {

WebRtcEndpointChannel::WebRtcEndpointChannel(
    const std::string& service_id, const std::string& channel_name,
    mediums::WebRtcSocketWrapper socket)
    : BaseEndpointChannel(service_id, channel_name, &socket.GetInputStream(),
                          &socket.GetOutputStream()),
      webrtc_socket_(std::move(socket)) {}

location::nearby::proto::connections::Medium WebRtcEndpointChannel::GetMedium()
    const {
  return location::nearby::proto::connections::Medium::WEB_RTC;
}

void WebRtcEndpointChannel::CloseImpl() { webrtc_socket_.Close(); }

}  // namespace connections
}  // namespace nearby
