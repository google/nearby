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

#ifndef CORE_INTERNAL_WEBRTC_ENDPOINT_CHANNEL_H_
#define CORE_INTERNAL_WEBRTC_ENDPOINT_CHANNEL_H_

#include <string>

#include "connections/implementation/base_endpoint_channel.h"
#ifdef NO_WEBRTC
#include "connections/implementation/mediums/webrtc_socket_stub.h"
#else
#include "connections/implementation/mediums/webrtc_socket.h"
#endif

namespace location {
namespace nearby {
namespace connections {

class WebRtcEndpointChannel final : public BaseEndpointChannel {
 public:
  WebRtcEndpointChannel(const std::string& service_id,
                        const std::string& channel_name,
                        mediums::WebRtcSocketWrapper webrtc_socket);

  proto::connections::Medium GetMedium() const override;

 private:
  void CloseImpl() override;

  mediums::WebRtcSocketWrapper webrtc_socket_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_WEBRTC_ENDPOINT_CHANNEL_H_
