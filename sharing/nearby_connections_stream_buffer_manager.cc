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
#include "sharing/nearby_connections_stream_buffer_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/meta/type_traits.h"
#include "sharing/internal/public/logging.h"

namespace nearby {
namespace sharing {

NearbyConnectionsStreamBufferManager::PayloadWithBuffer::PayloadWithBuffer(
    NcPayload payload)
    : buffer_payload(std::move(payload)) {}

NearbyConnectionsStreamBufferManager::NearbyConnectionsStreamBufferManager() =
    default;

NearbyConnectionsStreamBufferManager::~NearbyConnectionsStreamBufferManager() =
    default;

void NearbyConnectionsStreamBufferManager::StartTrackingPayload(
    NcPayload payload) {
  int64_t payload_id = payload.GetId();
  NL_LOG(INFO) << "Starting to track stream payload with ID " << payload_id;

  id_to_payload_with_buffer_map_[payload_id] =
      std::make_unique<PayloadWithBuffer>(std::move(payload));
}

bool NearbyConnectionsStreamBufferManager::IsTrackingPayload(
    int64_t payload_id) const {
  return id_to_payload_with_buffer_map_.contains(payload_id);
}

void NearbyConnectionsStreamBufferManager::StopTrackingFailedPayload(
    int64_t payload_id) {
  id_to_payload_with_buffer_map_.erase(payload_id);
  NL_LOG(INFO) << "Stopped tracking payload with ID " << payload_id << " "
               << "and cleared internal memory.";
}

void NearbyConnectionsStreamBufferManager::HandleBytesTransferred(
    int64_t payload_id, int64_t cumulative_bytes_transferred_so_far) {
  auto it = id_to_payload_with_buffer_map_.find(payload_id);
  if (it == id_to_payload_with_buffer_map_.end()) {
    NL_LOG(ERROR) << "Attempted to handle stream bytes for payload with ID "
                  << payload_id << ", but this payload was not being tracked.";
    return;
  }

  PayloadWithBuffer* payload_with_buffer = it->second.get();

  // We only need to read the new bytes which have not already been inserted
  // into the buffer.
  size_t bytes_to_read =
      cumulative_bytes_transferred_so_far - payload_with_buffer->buffer.size();

  NcInputStream* stream = payload_with_buffer->buffer_payload.AsStream();
  if (!stream) {
    NL_LOG(ERROR) << "Payload with ID " << payload_id << " is not a stream "
                  << "payload; transfer has failed.";
    StopTrackingFailedPayload(payload_id);
    return;
  }

  NcExceptionOr<NcByteArray> bytes = stream->Read(bytes_to_read);
  if (!bytes.ok()) {
    NL_LOG(ERROR) << "Payload with ID " << payload_id << " encountered "
                  << "exception while reading; transfer has failed.";
    StopTrackingFailedPayload(payload_id);
    return;
  }
  // Empty `bytes` means the End Of File. There should be at `bytes_to_read`
  // bytes available in the input stream, so we should never face the EOF
  // condition.
  NL_DCHECK(!bytes.result().Empty());

  payload_with_buffer->buffer += static_cast<std::string>(bytes.result());
}

NcByteArray
NearbyConnectionsStreamBufferManager::GetCompletePayloadAndStopTracking(
    int64_t payload_id) {
  auto it = id_to_payload_with_buffer_map_.find(payload_id);
  if (it == id_to_payload_with_buffer_map_.end()) {
    NL_LOG(ERROR) << "Attempted to get complete payload with ID " << payload_id
                  << ", but this payload was not being tracked.";
    return NcByteArray();
  }

  NcByteArray complete_payload(it->second->buffer);

  // Close stream and erase internal state before returning payload.
  it->second->buffer_payload.AsStream()->Close();
  id_to_payload_with_buffer_map_.erase(it);

  return complete_payload;
}

}  // namespace sharing
}  // namespace nearby
