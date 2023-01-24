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

#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_LOCAL_ICE_CANDIDATE_LISTENER_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_LOCAL_ICE_CANDIDATE_LISTENER_H_

#include "connections/listeners.h"
#include "webrtc/api/peer_connection_interface.h"

namespace nearby {
namespace connections {
namespace mediums {

// Callbacks from local ice candidate collection.
struct LocalIceCandidateListener {
  // Called when a new local ice candidate has been found.
  absl::AnyInvocable<void(const webrtc::IceCandidateInterface*)>
      local_ice_candidate_found_cb =
          nearby::DefaultCallback<const webrtc::IceCandidateInterface*>();
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_LOCAL_ICE_CANDIDATE_LISTENER_H_
