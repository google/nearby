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

#ifndef CORE_INTERNAL_PENDING_PAYLOAD_H_
#define CORE_INTERNAL_PENDING_PAYLOAD_H_

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/listeners.h"
#include "proto/connections_enums.pb.h"

namespace nearby::connections {

inline constexpr absl::Duration kMinTransferUpdateInterval =
    absl::Milliseconds(50);

// Tracks state for an InternalPayload.
class PendingPayload {
 public:
  PendingPayload();
  virtual ~PendingPayload() = default;

  PendingPayload(const PendingPayload&) = delete;
  PendingPayload& operator=(const PendingPayload&) = delete;

  bool IsLocallyCanceled() const;
  void MarkLocallyCanceledAndClose();

  // Records the point at which the transfer of this payload actually
  // started, used to compute the transfer duration and throughput that are
  // logged once the payload completes.
  //
  // `start_time` is when the transfer started (e.g. when the payload
  // executor begins sending an outgoing payload, rather than when it was
  // queued; or when the first chunk of an incoming payload arrived).
  // `start_offset` is the number of payload bytes that were already
  // transferred at `start_time` and should therefore be excluded from the
  // throughput computation (e.g. the resume offset of an outgoing payload,
  // or the body of the first chunk of an incoming payload, which had
  // already been received when the start time was stamped).
  //
  // If never called, the start time defaults to when this pending payload
  // was created and the start offset defaults to 0.
  void SetTransferStart(absl::Time start_time, int64_t start_offset)
      ABSL_LOCKS_EXCLUDED(mutex_);
  absl::Time GetTransferStartTime() const ABSL_LOCKS_EXCLUDED(mutex_);
  int64_t GetTransferStartOffset() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Closes the underlying payload.
  // Close is called when a pending payload does not have associated
  // endpoints.
  virtual void Close() = 0;

 private:
  mutable absl::Mutex mutex_;
  std::atomic<bool> is_locally_canceled_{false};
  absl::Time transfer_start_time_ ABSL_GUARDED_BY(mutex_);
  int64_t transfer_start_offset_ ABSL_GUARDED_BY(mutex_) = 0;
};

PayloadProgressInfo::Status PayloadStatusToTransferUpdateStatus(
    location::nearby::proto::connections::PayloadStatus status);

void SendControlMessage(
    EndpointManager& endpoint_manager,
    const std::vector<std::string>& endpoint_ids,
    const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
        payload_header,
    int64_t num_bytes_successfully_transferred,
    location::nearby::connections::PayloadTransferFrame::ControlMessage::
        EventType event_type);

std::string FormatPayloadTransferSummary(
    bool is_incoming, absl::string_view endpoint_id,
    const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
        payload_header,
    int64_t total_bytes, int64_t start_offset, absl::Duration duration,
    location::nearby::proto::connections::Medium medium);

// Logs a one-line summary when a payload finishes successfully. `end_time` is
// when the last chunk was handled on the transfer thread, so the duration is
// not skewed by the status update thread's queueing delay. See
// `FormatPayloadTransferSummary()` for the other parameters.
void LogPayloadTransferSummary(
    bool is_incoming, absl::string_view endpoint_id,
    const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
        payload_header,
    int64_t total_bytes, absl::Time start_time, int64_t start_offset,
    absl::Time end_time, location::nearby::proto::connections::Medium medium);

}  // namespace nearby::connections

#endif  // CORE_INTERNAL_PENDING_PAYLOAD_H_
