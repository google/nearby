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

#ifndef PLATFORM_V2_API_WEBRTC_H_
#define PLATFORM_V2_API_WEBRTC_H_

#include <vector>

#include "platform_v2/base/byte_array.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace api {

class WebRtcSignalingMessenger {
 public:
  virtual ~WebRtcSignalingMessenger() = default;

  /** Called whenever we receive an inbox message from tachyon. */
  class SignalingMessageListener {
   public:
    virtual ~SignalingMessageListener() = default;

    virtual void OnSignalingMessage(const ByteArray& message) = 0;
  };

  class IceServersListener {
   public:
    virtual ~IceServersListener() = default;

    virtual void OnIceServersFetched(
        std::vector<webrtc::PeerConnectionInterface::IceServer>
            ice_servers) = 0;
  };

  virtual bool RegisterSignaling() = 0;
  virtual bool UnregisterSignaling() = 0;
  virtual bool SendMessage(std::string_view peer_id,
                           const ByteArray& message) = 0;
  virtual bool StartReceivingMessages(
      const SignalingMessageListener& listener) = 0;
  virtual void GetIceServers(
      const IceServersListener& ice_servers_listener) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_WEBRTC_H_
