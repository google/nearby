// Copyright 2022 Google LLC
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
// limitations under the License

#ifndef THIRD_PARTY_NEARBY_SHARING_ENDPOINT_DISCOVERY_LISTENER_H_
#define THIRD_PARTY_NEARBY_SHARING_ENDPOINT_DISCOVERY_LISTENER_H_

#include "absl/strings/string_view.h"
#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {

// Listener invoked during endpoint discovery. This interface is used by the
// browser process to listen for a remote endpoint's status during endpoint
// discovery.
class EndpointDiscoveryListener {
 public:
  virtual ~EndpointDiscoveryListener() = default;

  // Called when a remote endpoint is discovered.
  //
  // endpoint_id - The ID of the remote endpoint that was discovered.
  // info        - Further information about the remote endpoint which may
  //               include the human-readable name if it is advertising in high
  //               visibility mode.
  virtual void OnEndpointFound(absl::string_view endpoint_id,
                               DiscoveredEndpointInfo& info) = 0;

  // Called when a remote endpoint is no longer discoverable; only called for
  // endpoints that previously had been passed to OnEndpointFound().
  //
  // endpoint_id - The ID of the remote endpoint that was lost.
  virtual void OnEndpointLost(absl::string_view endpoint_id) = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_ENDPOINT_DISCOVERY_LISTENER_H_
