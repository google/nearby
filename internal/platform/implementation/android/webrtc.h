// Copyright 2024 Google LLC
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

#ifndef PLATFORM_IMPL_ANDROID_WEBRTC_H_
#define PLATFORM_IMPL_ANDROID_WEBRTC_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/webrtc.h"
#include "webrtc/api/peer_connection_interface.h"

namespace nearby {
namespace android {

class WebRtcSignalingMessenger : public api::WebRtcSignalingMessenger {
 public:
  using OnSignalingMessageCallback =
      api::WebRtcSignalingMessenger::OnSignalingMessageCallback;
  using OnSignalingCompleteCallback =
      api::WebRtcSignalingMessenger::OnSignalingCompleteCallback;

  explicit WebRtcSignalingMessenger(
      absl::string_view self_id,
      const location::nearby::connections::LocationHint& location_hint);
  ~WebRtcSignalingMessenger() override = default;

  bool SendMessage(absl::string_view peer_id,
                   const ByteArray& message) override;
  bool StartReceivingMessages(
      OnSignalingMessageCallback on_message_callback,
      OnSignalingCompleteCallback on_complete_callback) override;
  void StopReceivingMessages() override;
};

class WebRtcMedium : public api::WebRtcMedium {
 public:
  using PeerConnectionCallback = api::WebRtcMedium::PeerConnectionCallback;

  WebRtcMedium() = default;
  ~WebRtcMedium() override;

  const std::string GetDefaultCountryCode() override;

  // Creates and returns a new webrtc::PeerConnectionInterface object via
  // |callback|.
  void CreatePeerConnection(webrtc::PeerConnectionObserver* observer,
                            PeerConnectionCallback callback) override;

  // Returns a signaling messenger for sending WebRTC signaling messages.
  std::unique_ptr<api::WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id,
      const location::nearby::connections::LocationHint& location_hint)
      override;
};

}  // namespace android
}  // namespace nearby

#endif  // PLATFORM_IMPL_ANDROID_WEBRTC_H_
