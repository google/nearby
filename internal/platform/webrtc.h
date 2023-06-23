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

#ifndef PLATFORM_PUBLIC_WEBRTC_H_
#define PLATFORM_PUBLIC_WEBRTC_H_

#ifndef NO_WEBRTC

#include <memory>

#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/webrtc.h"
#include "webrtc/api/peer_connection_interface.h"

namespace nearby {

class WebRtcSignalingMessenger {
 public:
  using OnSignalingMessageCallback =
      api::WebRtcSignalingMessenger::OnSignalingMessageCallback;
  using OnSignalingCompleteCallback =
      api::WebRtcSignalingMessenger::OnSignalingCompleteCallback;

  explicit WebRtcSignalingMessenger(
      std::unique_ptr<api::WebRtcSignalingMessenger> messenger)
      : impl_(std::move(messenger)) {}
  virtual ~WebRtcSignalingMessenger() = default;
  WebRtcSignalingMessenger(WebRtcSignalingMessenger&&) = default;
  WebRtcSignalingMessenger operator=(WebRtcSignalingMessenger&&) = delete;

  virtual bool SendMessage(absl::string_view peer_id,
                           const ByteArray& message) {
    return impl_->SendMessage(peer_id, message);
  }

  virtual bool StartReceivingMessages(
      OnSignalingMessageCallback on_message_callback,
      OnSignalingCompleteCallback on_complete_callback) {
    return impl_->StartReceivingMessages(std::move(on_message_callback),
                                         std::move(on_complete_callback));
  }

  virtual void StopReceivingMessages() { impl_->StopReceivingMessages(); }

  virtual bool IsValid() const { return impl_ != nullptr; }

 private:
  std::unique_ptr<api::WebRtcSignalingMessenger> impl_;
};

class WebRtcMedium {
 public:
  using PeerConnectionCallback = api::WebRtcMedium::PeerConnectionCallback;

  WebRtcMedium() : impl_(api::ImplementationPlatform::CreateWebRtcMedium()) {}
  virtual ~WebRtcMedium() = default;
  WebRtcMedium(WebRtcMedium&&) = delete;
  WebRtcMedium& operator=(WebRtcMedium&&) = delete;

  // Gets the default two-letter country code associated with current locale.
  // For example, en_US locale resolves to "US".
  const std::string GetDefaultCountryCode() {
    return impl_->GetDefaultCountryCode();
  }

  // Creates and returns a new webrtc::PeerConnectionInterface object via
  // |callback|.
  void CreatePeerConnection(webrtc::PeerConnectionObserver* observer,
                            PeerConnectionCallback callback) {
    impl_->CreatePeerConnection(observer, std::move(callback));
  }

  // Returns a signaling messenger for sending WebRTC signaling messages.
  virtual std::unique_ptr<WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id,
      const location::nearby::connections::LocationHint& location_hint) {
    return std::make_unique<WebRtcSignalingMessenger>(
        impl_->GetSignalingMessenger(self_id, location_hint));
  }

  virtual bool IsValid() const { return impl_ != nullptr; }

 private:
  std::unique_ptr<api::WebRtcMedium> impl_;
};

}  // namespace nearby

#endif

#endif  // PLATFORM_PUBLIC_WEBRTC_H_
