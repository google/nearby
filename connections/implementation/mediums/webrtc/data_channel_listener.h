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

#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_DATA_CHANNEL_LISTENER_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_DATA_CHANNEL_LISTENER_H_

#ifndef NO_WEBRTC

#include "absl/functional/any_invocable.h"
#include "connections/implementation/mediums/webrtc_socket.h"

namespace nearby {
namespace connections {
namespace mediums {

// Callbacks from the data channel.
struct DataChannelListener {
  // Called when the data channel is open and the socket wrapper is ready to
  // read and write.
  absl::AnyInvocable<void(WebRtcSocketWrapper)> data_channel_open_cb =
      [](WebRtcSocketWrapper) {};

  // Called when the data channel is closed.
  absl::AnyInvocable<void()> data_channel_closed_cb = []() {};
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_DATA_CHANNEL_LISTENER_H_
