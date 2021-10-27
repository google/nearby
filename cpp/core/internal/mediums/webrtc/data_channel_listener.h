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

#include "core/internal/mediums/webrtc_socket_wrapper.h"
#include "core/listeners.h"
#include "platform/base/byte_array.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// Callbacks from the data channel.
struct DataChannelListener {
  // Called when the data channel is open and the socket wraper is ready to
  // read and write.
  std::function<void(WebRtcSocketWrapper)> data_channel_open_cb =
      DefaultCallback<WebRtcSocketWrapper>();

  // Called when the data channel is closed.
  std::function<void()> data_channel_closed_cb = DefaultCallback<>();
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_DATA_CHANNEL_LISTENER_H_
