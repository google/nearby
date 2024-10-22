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

#ifndef THIRD_PARTY_NEARBY_SHARING_CONNECTION_LIFECYCLE_LISTENER_H_
#define THIRD_PARTY_NEARBY_SHARING_CONNECTION_LIFECYCLE_LISTENER_H_

#include "absl/strings/string_view.h"
#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {

// Listener for lifecycle events associated with a connection to a remote
// endpoint. Methods in this interface are called from the utility process, and
// are used by the browser process to listen for connection status associated
// with remote endpoints.
class ConnectionLifecycleListener {
 public:
  virtual ~ConnectionLifecycleListener() = default;

  // A basic encrypted channel has been created between this device and the
  //  remote endpoint. Both sides are now asked if they wish to accept or
  // reject the connection before any data can be sent over this channel.
  //
  // Optionally, the caller can verify if this device is connected to the
  // correct remote before accepting the connection. Typically, this involves
  // showing ConnectionInfo::authentication_token on both devices and having the
  // users manually compare and confirm. Both devices are given an identical
  // authentication token.
  //
  // Call NearbyConnections::AcceptConnection() to accept the connection, or
  // NearbyConnections::RejectConnection() to close the connection.
  //
  // endpoint_id - The identifier for the remote endpoint.
  // info        -  Other relevant information about the connection.
  virtual void OnConnectionInitiated(absl::string_view endpoint_id,
                                     ConnectionInfo info) = 0;

  // Called after both sides have accepted the connection.
  //
  // endpoint_id - The identifier for the remote endpoint.
  virtual void OnConnectionAccepted(absl::string_view endpoint_id) = 0;

  // Called when either side rejected the connection.
  // Call NearbyConnections::DisconnectFromEndpoint() to terminate connection.
  //
  // endpoint_id - The identifier for the remote endpoint.
  // status      - The result of the connection. Valid values are
  //               Status::kSuccess and Status::kConnectionRejected}.
  virtual void OnConnectionRejected(absl::string_view endpoint_id,
                                    Status status) = 0;

  // Called when a remote endpoint is disconnected or has become unreachable.
  // At this point service (re-)discovery may start again.
  //
  // endpoint_id - The identifier for the remote endpoint.
  virtual void OnDisconnected(absl::string_view endpoint_id) = 0;

  // Called when the connection's available bandwidth has changed.
  //
  // endpoint_id - The identifier for the remote endpoint.
  // quality     - The new quality for the connection.
  virtual void OnBandwidthChanged(absl::string_view endpoint_id,
                                  Medium medium) = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CONNECTION_LIFECYCLE_LISTENER_H_
