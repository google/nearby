// Copyright 2025 Google LLC
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

#ifndef PLATFORM_IMPL_APPLE_WEBRTC_H_
#define PLATFORM_IMPL_APPLE_WEBRTC_H_

#ifndef NO_WEBRTC

#include <memory>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/webrtc.h"
#include "webrtc/api/peer_connection_interface.h"

namespace nearby::apple {

class WebRtcMedium : public api::WebRtcMedium {
 public:
  ~WebRtcMedium() override = default;

  // Gets the default two-letter country code associated with current locale.
  // For example, en_US locale resolves to "US".
  // This follows the ISO 3166-1 Alpha-2 standard.
  std::string GetDefaultCountryCode() override;

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
};

}  // namespace nearby::apple

#endif  // #ifndef NO_WEBRTC

#endif  // PLATFORM_IMPL_APPLE_WEBRTC_H_
