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

#ifndef PLATFORM_V2_IMPL_G3_WEBRTC_H_
#define PLATFORM_V2_IMPL_G3_WEBRTC_H_

#include <memory>

#include "platform_v2/api/webrtc.h"
#include "absl/strings/string_view.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace g3 {

class WebRtcSignalingMessenger : public api::WebRtcSignalingMessenger {
 public:
  using OnSignalingMessageCallback =
      api::WebRtcSignalingMessenger::OnSignalingMessageCallback;

  explicit WebRtcSignalingMessenger(absl::string_view self_id);
  ~WebRtcSignalingMessenger() override = default;

  bool SendMessage(absl::string_view peer_id,
                   const ByteArray& message) override;
  bool StartReceivingMessages(OnSignalingMessageCallback listener) override;
  void StopReceivingMessages() override;

 private:
  absl::string_view self_id_;
};

class WebRtcMedium : public api::WebRtcMedium {
 public:
  using PeerConnectionCallback = api::WebRtcMedium::PeerConnectionCallback;

  WebRtcMedium() = default;
  ~WebRtcMedium() override = default;

  // Creates and returns a new webrtc::PeerConnectionInterface object via
  // |callback|.
  void CreatePeerConnection(webrtc::PeerConnectionObserver* observer,
                            PeerConnectionCallback callback) override;

  // Returns a signaling messenger for sending WebRTC signaling messages.
  std::unique_ptr<api::WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id) override;
 private:
  std::unique_ptr<rtc::Thread> signaling_thread_;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_G3_WEBRTC_H_
