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

#ifndef PLATFORM_IMPL_G3_WEBRTC_H_
#define PLATFORM_IMPL_G3_WEBRTC_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/webrtc.h"
#include "internal/platform/implementation/g3/single_thread_executor.h"
#include "webrtc/api/peer_connection_interface.h"

namespace nearby {
namespace g3 {

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

 private:
  std::string self_id_;
  location::nearby::connections::LocationHint location_hint_;
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

  // Creates and returns a new webrtc::PeerConnectionInterface object via
  // |callback| with |PeerConnectionFactoryInterface::Options|.
  void CreatePeerConnection(
      std::optional<webrtc::PeerConnectionFactoryInterface::Options> options,
      webrtc::PeerConnectionObserver* observer,
      PeerConnectionCallback callback) override;

  // Returns a signaling messenger for sending WebRTC signaling messages.
  std::unique_ptr<api::WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id,
      const location::nearby::connections::LocationHint& location_hint)
      override;

 private:
  // Executor for handling calls to create a peer connection.
  SingleThreadExecutor single_thread_executor_;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_WEBRTC_H_
