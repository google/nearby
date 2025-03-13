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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_WEBRTC_STATE_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_WEBRTC_STATE_H_

namespace nearby {
namespace connections {

// Represents the WebRtc state that mediums are connectable or not.
enum class WebRtcState {
  kUndefined = 0,
  kConnectable = 1,
  kUnconnectable = 2,
};

}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_WEBRTC_STATE_H_
