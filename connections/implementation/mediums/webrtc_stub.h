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

#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_STUB_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_STUB_H_

#ifdef NO_WEBRTC

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "connections/implementation/mediums/webrtc_peer_id_stub.h"
#include "connections/implementation/mediums/webrtc_socket_stub.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/listeners.h"

namespace nearby {
namespace connections {
namespace mediums {

// Entry point for connecting a data channel between two devices via WebRtc.
class WebRtc {
 public:
  // Callback that is invoked when a new connection is accepted.
  using AcceptedConnectionCallback =
      absl::AnyInvocable<void(WebRtcSocketWrapper socket)>;
  WebRtc();
  ~WebRtc();

  // Gets the default two-letter country code associated with current locale.
  // For example, en_US locale resolves to "US".
  const std::string GetDefaultCountryCode();

  // Returns if WebRtc is available as a medium for nearby to transport data.
  // Runs on @MainThread.
  bool IsAvailable();

  // Returns if the device is accepting connection with specific service id.
  // Runs on @MainThread.
  bool IsAcceptingConnections(const std::string& service_id);

  // Prepares the device to accept incoming WebRtc connections. Returns a
  // boolean value indicating if the device has started accepting connections.
  // Runs on @MainThread.
  bool StartAcceptingConnections(
      const std::string& service_id, const WebrtcPeerId& self_peer_id,
      const location::nearby::connections::LocationHint& location_hint,
      AcceptedConnectionCallback callback);

  // Try to stop (accepting) the specific connection with provided service id.
  // Runs on @MainThread
  void StopAcceptingConnections(const std::string& service_id);

  // Initiates a WebRtc connection with peer device identified by |peer_id|
  // with internal retry for maximum attempts of kConnectAttemptsLimit.
  // Runs on @MainThread.
  WebRtcSocketWrapper Connect(
      const std::string& service_id, const WebrtcPeerId& peer_id,
      const location::nearby::connections::LocationHint& location_hint,
      CancellationFlag* cancellation_flag);

  bool IsUsingCellular();
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_STUB_H_
