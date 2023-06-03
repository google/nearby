// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_V3_LISTENERS_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_V3_LISTENERS_H_

#include <functional>

#include "absl/functional/any_invocable.h"
#include "connections/listeners.h"
#include "connections/v3/bandwidth_info.h"
#include "connections/v3/connection_result.h"
#include "internal/interop/device.h"

namespace nearby {
namespace connections {
namespace v3 {

struct ConnectionListener {
  // A basic encrypted channel has been created between you and the endpoint.
  // Both sides are now asked if they wish to accept or reject the connection
  // before any data can be sent over this channel.
  //
  // This is your chance, before you accept the connection, to confirm that you
  // connected to the correct device. Both devices are given an identical token;
  // it's up to you to decide how to verify it before proceeding. Typically this
  // involves showing the token on both devices and having the users manually
  // compare and confirm; however, this is only required if you desire a secure
  // connection between the devices.
  //
  // Whichever route you decide to take (including not authenticating the other
  // device), call Core::AcceptConnection() when you're ready to talk, or
  // Core::RejectConnection() to close the connection.
  //
  // remote_device - The identifier for the remote endpoint.
  // info -  Other relevant information about the connection.
  absl::AnyInvocable<void(const NearbyDevice& remote_device,
                          const v3::InitialConnectionInfo& info)>
      initiated_cb =
          [](const NearbyDevice&, const v3::InitialConnectionInfo&) {};

  // Called when both sides have accepted or either side has rejected the
  // connection. If the {@link ConnectionResolution}'s status is {@link
  // ConnectionsStatusCodes#SUCCESS}, both sides have accepted the
  // connection and may now send {@link Payload}s to each other.
  // Otherwise, the connection was rejected.
  // We use std::function here as both rejection and acceptance callbacks call
  // this function now.

  // remote_device - The identifier for the remote endpoint.
  // resolution    - The resolution of the connection (accepted or rejected).
  std::function<void(const NearbyDevice& remote_device,
                     ConnectionResult resolution)>
      result_cb = [](const NearbyDevice&, ConnectionResult) {};

  // Called when a remote endpoint is disconnected or has become unreachable.
  // At this point service (re-)discovery may start again.
  //
  // remote_device - The identifier for the remote endpoint.
  absl::AnyInvocable<void(const NearbyDevice& remote_device)> disconnected_cb =
      [](const NearbyDevice&) {};

  // Called when the connection's available bandwidth has changed.
  //
  // remote_device - The identifier for the remote endpoint.
  // bandwidth_info - Bandwidth info about the new medium that was upgraded to.
  absl::AnyInvocable<void(const NearbyDevice& remote_device,
                          BandwidthInfo bandwidth_info)>
      bandwidth_changed_cb = [](const NearbyDevice&, BandwidthInfo) {};
};

struct DiscoveryListener {
  // Called when a remote endpoint is discovered.
  //
  // remote_device - The remote device that was discovered.
  // service_id    - The ID of the service advertised by the remote endpoint.
  absl::AnyInvocable<void(const NearbyDevice& remote_device,
                          const absl::string_view service_id)>
      endpoint_found_cb = [](const NearbyDevice&, const absl::string_view) {};

  // Called when a remote endpoint is no longer discoverable; only called for
  // endpoints that previously had been passed to {@link
  // #onEndpointFound(String, DiscoveredEndpointInfo)}.
  //
  // remote_device - The ID of the remote endpoint that was lost.
  absl::AnyInvocable<void(const NearbyDevice& remote_device)> endpoint_lost_cb =
      [](const NearbyDevice&) {};

  // Called when a remote endpoint is found with an updated distance.
  //
  // arguments:
  //   remote_device - The ID of the remote endpoint that was lost.
  //   info          - The distance info, encoded as enum value.
  absl::AnyInvocable<void(const NearbyDevice& remote_device, DistanceInfo info)>
      endpoint_distance_changed_cb = [](const NearbyDevice&, DistanceInfo) {};
};

struct PayloadListener {
  // Called when a Payload is received from a remote endpoint. Depending
  // on the type of the Payload, all of the data may or may not have been
  // received at the time of this call. Use OnPayloadProgress() to
  // get updates on the status of the data received.
  //
  // remote_device - The identifier for the remote endpoint that sent the
  //               payload.
  // payload     - The Payload object received.
  absl::AnyInvocable<void(const NearbyDevice& remote_device, Payload payload)
                         const>
      payload_received_cb = [](const NearbyDevice&, Payload) {};

  // Called with progress information about an active Payload transfer, either
  // incoming or outgoing.
  //
  // remote_device - The identifier for the remote endpoint that is sending or
  //               receiving this payload.
  // info -  The PayloadProgressInfo structure describing the status of
  //         the transfer.
  absl::AnyInvocable<void(const NearbyDevice& remote_device,
                          const PayloadProgressInfo& info)>
      payload_progress_cb =
          [](const NearbyDevice&, const PayloadProgressInfo&) {};
};

}  // namespace v3
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_V3_LISTENERS_H_
