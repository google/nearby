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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_STREAM_BUFFER_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_STREAM_BUFFER_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "connections/core.h"
#include "connections/payload.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"

namespace nearby {
namespace sharing {

using NcByteArray = ::nearby::ByteArray;
using NcException = ::nearby::Exception;
template <class T>
using NcExceptionOr = ::nearby::ExceptionOr<T>;
using NcInputStream = ::nearby::InputStream;
using NcPayload = ::nearby::connections::Payload;

// Manages payloads with type "stream" received over Nearby Connections. Streams
// over a certain size are delivered in chunks and need to be reassembled upon
// completion.
//
// Clients should start tracking a payload via StartTrackingPayload(). When
// more bytes have been transferred, clients should invoke
// HandleBytesTransferred(), passing the cumulative number of bytes that have
// been transferred. When all bytes have finished being transferred, clients
// should invoke GetCompletePayloadAndStopTracking() to get the complete,
// reassembled payload.
//
// If a payload has failed or been canceled, clients should invoke
// StopTrackingFailedPayload() so that this class can clean up its internal
// buffer.
class NearbyConnectionsStreamBufferManager {
 public:
  NearbyConnectionsStreamBufferManager();
  ~NearbyConnectionsStreamBufferManager();

  // Starts tracking the given payload.
  void StartTrackingPayload(NcPayload payload);

  // Returns whether a payload with the provided ID is being tracked.
  bool IsTrackingPayload(int64_t payload_id) const;

  // Stops tracking the payload with the provided ID and cleans up internal
  // memory being used to buffer the partially-completed transfer.
  void StopTrackingFailedPayload(int64_t payload_id);

  // Processes incoming bytes by reading from the input stream.
  void HandleBytesTransferred(int64_t payload_id,
                              int64_t cumulative_bytes_transferred_so_far);

  // Returns the completed buffer and deletes internal buffers.
  NcByteArray GetCompletePayloadAndStopTracking(int64_t payload_id);

 private:
  struct PayloadWithBuffer {
    explicit PayloadWithBuffer(NcPayload payload);

    NcPayload buffer_payload;

    // Partially-complete buffer which contains the bytes which have been read
    // up to this point.
    std::string buffer;
  };

  absl::flat_hash_map<int64_t, std::unique_ptr<PayloadWithBuffer>>
      id_to_payload_with_buffer_map_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_STREAM_BUFFER_MANAGER_H_
