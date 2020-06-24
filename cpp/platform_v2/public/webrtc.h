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

#ifndef PLATFORM_V2_PUBLIC_WEBRTC_H_
#define PLATFORM_V2_PUBLIC_WEBRTC_H_

#include <memory>

#include "platform_v2/api/platform.h"
#include "platform_v2/api/webrtc.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {

class WebRtcSignalingMessenger final {
 public:
  using OnSignalingMessageCallback =
      api::WebRtcSignalingMessenger::OnSignalingMessageCallback;

  explicit WebRtcSignalingMessenger(
      std::unique_ptr<api::WebRtcSignalingMessenger> messenger)
      : impl_(std::move(messenger)) {}
  ~WebRtcSignalingMessenger() = default;
  WebRtcSignalingMessenger(WebRtcSignalingMessenger&&) = default;
  WebRtcSignalingMessenger operator=(WebRtcSignalingMessenger&&) = delete;

  bool SendMessage(absl::string_view peer_id, const ByteArray& message) {
    return impl_->SendMessage(peer_id, message);
  }

  bool StartReceivingMessages(OnSignalingMessageCallback listener) {
    return impl_->StartReceivingMessages(listener);
  }

  void StopReceivingMessages() { impl_->StopReceivingMessages(); }

  bool IsValid() const { return impl_ != nullptr; }

 private:
  std::unique_ptr<api::WebRtcSignalingMessenger> impl_;
};

class WebRtcMedium final {
 public:
  using PeerConnectionCallback = api::WebRtcMedium::PeerConnectionCallback;

  WebRtcMedium() : impl_(api::ImplementationPlatform::CreateWebRtcMedium()) {}
  ~WebRtcMedium() = default;
  WebRtcMedium(WebRtcMedium&&) = delete;
  WebRtcMedium& operator=(WebRtcMedium&&) = delete;

  // Creates and returns a new webrtc::PeerConnectionInterface object via
  // |callback|.
  void CreatePeerConnection(webrtc::PeerConnectionObserver* observer,
                            PeerConnectionCallback callback) {
    impl_->CreatePeerConnection(observer, std::move(callback));
  }

  // Returns a signaling messenger for sending WebRTC signaling messages.
  std::unique_ptr<WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id) {
    return std::make_unique<WebRtcSignalingMessenger>(
        impl_->GetSignalingMessenger(self_id));
  }

  bool IsValid() const { return impl_ != nullptr; }

 private:
  std::unique_ptr<api::WebRtcMedium> impl_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_WEBRTC_H_
