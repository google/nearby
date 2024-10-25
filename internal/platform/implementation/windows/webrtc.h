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

#include <memory>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/implementation/webrtc.h"
#include "webrtc/api/peer_connection_interface.h"

namespace nearby {
namespace windows {

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
  AccountManager* account_manager_;
};

class WebRtcMedium : public api::WebRtcMedium {
 public:
  // TODO(b/261663238): replace with real implementation.
  ~WebRtcMedium() override = default;

  // Gets the default two-letter country code associated with current locale.
  // For example, en_US locale resolves to "US".
  // This follows the ISO 3166-1 Alpha-2 standard.
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
  // TODO(b/261663238): replace with real implementation.
  std::unique_ptr<api::WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id,
      const location::nearby::connections::LocationHint& location_hint)
      override;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_WEBRTC_H_
