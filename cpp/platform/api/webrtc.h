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

#ifndef PLATFORM_API_WEBRTC_H_
#define PLATFORM_API_WEBRTC_H_

#include <vector>

#include "platform/byte_array.h"
#include "platform/ptr.h"
//#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
#if 0
class WebRtcSignalingMessenger {
 public:
  virtual ~WebRtcSignalingMessenger() = default;

  /** Called whenever we receive an inbox message from tachyon. */
  class SignalingMessageListener {
   public:
    virtual ~SignalingMessageListener() = default;

    virtual void onSignalingMessage(ConstPtr<ByteArray> message) = 0;
  };

  class IceServersListener {
   public:
    virtual ~IceServersListener() = default;

    virtual void OnIceServersFetched(
        std::vector<Ptr<webrtc::PeerConnectionInterface::IceServer>>
            ice_servers) = 0;
  };

  virtual bool registerSignaling() = 0;
  virtual bool unregisterSignaling() = 0;
  virtual bool sendMessage(const std::string& peer_id,
                           ConstPtr<ByteArray> message) = 0;
  virtual bool startReceivingMessages(
      Ptr<SignalingMessageListener> listener) = 0;
  virtual void getIceServers(Ptr<IceServersListener> ice_servers_listener) = 0;
};
#endif

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_WEBRTC_H_
