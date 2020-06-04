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

#include <memory>

#include "platform_v2/base/byte_array.h"
#include "absl/strings/string_view.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace api {

class WebRtcSignalingMessenger {
 public:
  using OnSignalingMessageCallback = std::function<void(const ByteArray&)>;

  virtual ~WebRtcSignalingMessenger() = default;

  virtual bool SendMessage(absl::string_view peer_id,
                           const ByteArray& message) = 0;

  virtual bool StartReceivingMessages(OnSignalingMessageCallback listener) = 0;
  virtual void StopReceivingMessages() = 0;
};

class WebRtcMedium {
 public:
  using PeerConnectionCallback =
      std::function<void(rtc::scoped_refptr<webrtc::PeerConnectionInterface>)>;

  virtual ~WebRtcMedium() = default;

  // Creates and returns a new webrtc::PeerConnectionInterface object via
  // |callback|.
  virtual void CreatePeerConnection(webrtc::PeerConnectionObserver* observer,
                                    PeerConnectionCallback callback) = 0;

  // Returns a signaling messenger for sending WebRTC signaling messages.
  virtual std::unique_ptr<WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_WEBRTC_H_
