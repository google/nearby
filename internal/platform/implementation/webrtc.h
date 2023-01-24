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

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "internal/platform/byte_array.h"
#include "webrtc/api/peer_connection_interface.h"

namespace nearby {
namespace api {

class WebRtcSignalingMessenger {
 public:
  using OnSignalingMessageCallback = absl::AnyInvocable<void(const ByteArray&)>;
  using OnSignalingCompleteCallback = absl::AnyInvocable<void(bool)>;

  virtual ~WebRtcSignalingMessenger() = default;

  virtual bool SendMessage(absl::string_view peer_id,
                           const ByteArray& message) = 0;

  virtual bool StartReceivingMessages(
      OnSignalingMessageCallback on_message_callback,
      OnSignalingCompleteCallback on_complete_callback) = 0;
  virtual void StopReceivingMessages() = 0;
};

class WebRtcMedium {
 public:
  using PeerConnectionCallback = absl::AnyInvocable<void(
      rtc::scoped_refptr<webrtc::PeerConnectionInterface>)>;

  virtual ~WebRtcMedium() = default;

  // Gets the default two-letter country code associated with current locale.
  // For example, en_US locale resolves to "US".
  virtual const std::string GetDefaultCountryCode() = 0;

  // Creates and returns a new webrtc::PeerConnectionInterface object via
  // |callback|.
  virtual void CreatePeerConnection(webrtc::PeerConnectionObserver* observer,
                                    PeerConnectionCallback callback) = 0;

  // Returns a signaling messenger for sending WebRTC signaling messages.
  virtual std::unique_ptr<WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id,
      const location::nearby::connections::LocationHint& location_hint) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_WEBRTC_H_
