// Copyright 2021 Google LLC
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

#ifndef CORE_LISTENERS_H_
#define CORE_LISTENERS_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

// This file defines all the protocol listeners and their parameter structures.
// Listeners are defined as collections of std::function<T> instances, which is
// more flexible than a virtual function:
// - a subset of listener callbacks may be overridden, while others may remain
//   default-initialized.
// - callbacks may be initialized with lambdas; lambda definitions are concize.

#include "connections/connection_options.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/byte_utils.h"

namespace nearby {
namespace connections {

// Common callback for asynchronously invoked methods.
// Called after a job scheduled for execution is completed.
// This is not the same as completion of the associated process,
// which may have many states, and multiple async jobs, and be still ongoing.
// Progress on the overall process is reported by the associated listener.
struct ResultCallback {
  // Callback to access the status of the operation when available.
  // status - result of job execution;
  //   Status::kSuccess, if successful; anything else indicates failure.
  std::function<void(Status)> result_cb = [](Status) {};
};

struct ConnectionResponseInfo {
  std::string GetAuthenticationDigits() {
    return ByteUtils::ToFourDigitString(raw_authentication_token);
  }

  ByteArray remote_endpoint_info;
  std::string authentication_token;
  ByteArray raw_authentication_token;
  bool is_incoming_connection = false;
  bool is_connection_verified = false;
};

struct PayloadProgressInfo {
  std::int64_t payload_id = 0;
  enum class Status {
    kSuccess,
    kFailure,
    kInProgress,
    kCanceled,
  } status = Status::kSuccess;
  std::int64_t total_bytes = 0;
  std::int64_t bytes_transferred = 0;
};

enum class DistanceInfo {
  kUnknown = 1,
  kVeryClose = 2,
  kClose = 3,
  kFar = 4,
};

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
  // endpoint_id - The identifier for the remote endpoint.
  // info -  Other relevant information about the connection.
  std::function<void(const std::string& endpoint_id,
                     const ConnectionResponseInfo& info)>
      initiated_cb = [](const std::string&, const ConnectionResponseInfo&) {};

  // Called after both sides have accepted the connection.
  // Both sides may now send Payloads to each other.
  // Call Core::SendPayload() or wait for incoming PayloadListener::OnPayload().
  //
  // endpoint_id - The identifier for the remote endpoint.
  std::function<void(const std::string& endpoint_id)> accepted_cb =
      [](const std::string&) {};

  // Called when either side rejected the connection.
  // Payloads can not be exchaged. Call Core::DisconnectFromEndpoint()
  // to terminate connection.
  //
  // endpoint_id - The identifier for the remote endpoint.
  std::function<void(const std::string& endpoint_id, Status status)>
      rejected_cb = [](const std::string&, Status) {};

  // Called when a remote endpoint is disconnected or has become unreachable.
  // At this point service (re-)discovery may start again.
  //
  // endpoint_id - The identifier for the remote endpoint.
  std::function<void(const std::string& endpoint_id)> disconnected_cb =
      [](const std::string&) {};

  // Called when the connection's available bandwidth has changed.
  //
  // endpoint_id - The identifier for the remote endpoint.
  // medium      - Medium we upgraded to.
  std::function<void(const std::string& endpoint_id, Medium medium)>
      bandwidth_changed_cb = [](const std::string&, Medium) {};
};

struct DiscoveryListener {
  // Called when a remote endpoint is discovered.
  //
  // endpoint_id   - The ID of the remote endpoint that was discovered.
  // endpoint_info - The info of the remote endpoint representd by ByteArray.
  // service_id    - The ID of the service advertised by the remote endpoint.
  std::function<void(const std::string& endpoint_id,
                     const ByteArray& endpoint_info,
                     const std::string& service_id)>
      endpoint_found_cb =
          [](const std::string&, const ByteArray&, const std::string&) {};

  // Called when a remote endpoint is no longer discoverable; only called for
  // endpoints that previously had been passed to {@link
  // #onEndpointFound(String, DiscoveredEndpointInfo)}.
  //
  // endpoint_id - The ID of the remote endpoint that was lost.
  std::function<void(const std::string& endpoint_id)> endpoint_lost_cb =
      [](const std::string&) {};

  // Called when a remote endpoint is found with an updated distance.
  //
  // arguments:
  //   endpoint_id - The ID of the remote endpoint that was lost.
  //   info        - The distance info, encoded as enum value.
  std::function<void(const std::string& endpoint_id, DistanceInfo info)>
      endpoint_distance_changed_cb = [](const std::string&, DistanceInfo) {};
};

struct PayloadListener {
  // Called when a Payload is received from a remote endpoint. Depending
  // on the type of the Payload, all of the data may or may not have been
  // received at the time of this call. Use OnPayloadProgress() to
  // get updates on the status of the data received.
  //
  // endpoint_id - The identifier for the remote endpoint that sent the
  //               payload.
  // payload     - The Payload object received.
  std::function<void(const std::string& endpoint_id, Payload payload)>
      payload_cb = [](const std::string&, Payload) {};

  // Called with progress information about an active Payload transfer, either
  // incoming or outgoing.
  //
  // endpoint_id - The identifier for the remote endpoint that is sending or
  //               receiving this payload.
  // info -  The PayloadProgressInfo structure describing the status of
  //         the transfer.
  std::function<void(const std::string& endpoint_id,
                     const PayloadProgressInfo& info)>
      payload_progress_cb =
          [](const std::string&, const PayloadProgressInfo&) {};
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_LISTENERS_H_
