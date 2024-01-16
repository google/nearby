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
// limitations under the License.

#ifndef THIRD_PARTY_NEARBY_SHARING_PAYLOAD_LISTENER_H_
#define THIRD_PARTY_NEARBY_SHARING_PAYLOAD_LISTENER_H_

#include "absl/strings/string_view.h"
#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {

// Listener for payload status. Methods in this interface are called from
// utility process, and are used by the browser process to listen for payload
// status associated with remote endpoints.
class PayloadListener {
 public:
  virtual ~PayloadListener() = default;

  // Called when a Payload is received from a remote endpoint. Depending on the
  // type of the Payload, all the data may or may not have been received at
  // the time of this call. OnPayloadTransferUpdate() should be used to get
  // updates on the status of the data received.
  //
  // endpoint_id - The identifier for the remote endpoint that sent the
  //               payload.
  // payload     - The Payload object received.
  virtual void OnPayloadReceived(absl::string_view endpoint_id,
                                 Payload& payload) = 0;

  // Called with progress information about an active Payload transfer, either
  // incoming or outgoing.
  //
  // endpoint_id - The identifier for the remote endpoint that is sending or
  //               receiving this payload.
  // update      - The PayloadTransferUpdate structure describing the status of
  //               the transfer.
  virtual void OnPayloadTransferUpdate(absl::string_view endpoint_id,
                                       PayloadTransferUpdate& update) = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_PAYLOAD_LISTENER_H_
