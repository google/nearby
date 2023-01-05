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

#include "connections/implementation/mediums/webrtc_stub.h"

#include <functional>
#include <memory>

#include "connections/implementation/mediums/webrtc_socket_stub.h"
#include "internal/platform/listeners.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/future.h"

namespace nearby {
namespace connections {
namespace mediums {
using ::location::nearby::connections::LocationHint;

WebRtc::WebRtc() = default;

WebRtc::~WebRtc() {}

const std::string WebRtc::GetDefaultCountryCode() { return "US"; }

bool WebRtc::IsAvailable() { return false; }

bool WebRtc::IsAcceptingConnections(const std::string& service_id) {
  return false;
}

bool WebRtc::StartAcceptingConnections(const std::string& service_id,
                                       const WebrtcPeerId& self_peer_id,
                                       const LocationHint& location_hint,
                                       AcceptedConnectionCallback callback) {
  return false;
}

void WebRtc::StopAcceptingConnections(const std::string& service_id) {}

WebRtcSocketWrapper WebRtc::Connect(const std::string& service_id,
                                    const WebrtcPeerId& remote_peer_id,
                                    const LocationHint& location_hint,
                                    CancellationFlag* cancellation_flag) {
  return WebRtcSocketWrapper();
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
