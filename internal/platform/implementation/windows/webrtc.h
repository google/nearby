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

#ifndef PLATFORM_IMPL_WINDOWS_WEBRTC_H_
#define PLATFORM_IMPL_WINDOWS_WEBRTC_H_

#include "internal/platform/implementation/webrtc.h"

namespace nearby {
namespace windows {

class WebRtcSignalingMessenger : public api::WebRtcSignalingMessenger {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~WebRtcSignalingMessenger() override = default;

  // TODO(b/184975123): replace with real implementation.
  bool SendMessage(absl::string_view peer_id,
                   const ByteArray& message) override {
    return false;
  }

  // TODO(b/184975123): replace with real implementation.
  bool StartReceivingMessages(
      api::WebRtcSignalingMessenger::OnSignalingMessageCallback
          on_message_callback,
      api::WebRtcSignalingMessenger::OnSignalingCompleteCallback
          on_complete_callback) override {
    return false;
  }
  // TODO(b/184975123): replace with real implementation.
  void StopReceivingMessages() override {}
};

class WebRtcMedium : public api::WebRtcMedium {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~WebRtcMedium() override = default;

  // Gets the default two-letter country code associated with current locale.
  // For example, en_US locale resolves to "US".
  // TODO(b/184975123): replace with real implementation.
  const std::string GetDefaultCountryCode() override {
    return "Un-implemented";
  }

  // Creates and returns a new webrtc::PeerConnectionInterface object via
  // |callback|.
  // TODO(b/184975123): replace with real implementation.
  void CreatePeerConnection(webrtc::PeerConnectionObserver* observer,
                            PeerConnectionCallback callback) override {}

  // Returns a signaling messenger for sending WebRTC signaling messages.
  // TODO(b/184975123): replace with real implementation.
  std::unique_ptr<api::WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id,
      const location::nearby::connections::LocationHint& location_hint)
      override {
    return nullptr;
  }
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_WEBRTC_H_
