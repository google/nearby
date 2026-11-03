// Copyright 2021-2023 Google LLC
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

#include "connections/implementation/pending_payload.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/listeners.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "proto/connections_enums.pb.h"

namespace nearby::connections {

namespace {
using ::location::nearby::connections::PayloadTransferFrame;
using ::location::nearby::proto::connections::PayloadStatus;
}  // namespace

PendingPayload::PendingPayload()
    : transfer_start_time_(SystemClock::ElapsedRealtime()) {}

void PendingPayload::SetTransferStart(absl::Time start_time,
                                      int64_t start_offset) {
  absl::MutexLock lock(mutex_);
  transfer_start_time_ = start_time;
  transfer_start_offset_ = start_offset;
}

absl::Time PendingPayload::GetTransferStartTime() const {
  absl::MutexLock lock(mutex_);
  return transfer_start_time_;
}

int64_t PendingPayload::GetTransferStartOffset() const {
  absl::MutexLock lock(mutex_);
  return transfer_start_offset_;
}

bool PendingPayload::IsLocallyCanceled() const {
  return is_locally_canceled_.load();
}

void PendingPayload::MarkLocallyCanceledAndClose() {
  is_locally_canceled_.store(true);
  Close();
}

PayloadProgressInfo::Status PayloadStatusToTransferUpdateStatus(
    PayloadStatus status) {
  switch (status) {
    case PayloadStatus::LOCAL_CANCELLATION:
    case PayloadStatus::REMOTE_CANCELLATION:
      return PayloadProgressInfo::Status::kCanceled;
    case PayloadStatus::SUCCESS:
      return PayloadProgressInfo::Status::kSuccess;
    default:
      return PayloadProgressInfo::Status::kFailure;
  }
}

void SendControlMessage(
    EndpointManager& endpoint_manager,
    const std::vector<std::string>& endpoint_ids,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    int64_t num_bytes_successfully_transferred,
    PayloadTransferFrame::ControlMessage::EventType event_type) {
  PayloadTransferFrame::ControlMessage control_message;
  control_message.set_event(event_type);
  control_message.set_offset(num_bytes_successfully_transferred);

  endpoint_manager.SendControlMessage(payload_header, control_message,
                                      endpoint_ids);
}

std::string FormatPayloadTransferSummary(
    bool is_incoming, absl::string_view endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    int64_t total_bytes, int64_t start_offset, absl::Duration duration,
    location::nearby::proto::connections::Medium medium) {
  int64_t duration_ms =
      std::max<int64_t>(absl::ToInt64Milliseconds(duration), 0);
  int64_t measured_bytes = total_bytes - start_offset;
  // Throughput can't be meaningfully computed if none of the payload bytes
  // were transferred within the measured window (e.g. a single-chunk incoming
  // payload, whose only data chunk arrived before the start time was stamped).
  std::string throughput = "N/A";
  if (duration_ms > 0 && measured_bytes > 0) {
    int64_t throughput_kbps = (measured_bytes * 1000 / 1024) / duration_ms;
    // Show MB/s (with one decimal) for faster transfers, KB/s otherwise.
    throughput =
        throughput_kbps >= 1024
            ? absl::StrCat(throughput_kbps / 1024, ".",
                           (throughput_kbps % 1024) * 10 / 1024, " MB/s")
            : absl::StrCat(throughput_kbps, " KB/s");
  }
  return absl::StrCat(is_incoming ? "Received " : "Sent ",
                      PayloadTransferFrame::PayloadHeader::PayloadType_Name(
                          payload_header.type()),
                      " payload: payload_id=", payload_header.id(),
                      ", endpoint_id=", endpoint_id, ", medium=",
                      location::nearby::proto::connections::Medium_Name(medium),
                      ", size=", total_bytes, " bytes, duration=", duration_ms,
                      " ms, throughput=", throughput);
}

void LogPayloadTransferSummary(
    bool is_incoming, absl::string_view endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    int64_t total_bytes, absl::Time start_time, int64_t start_offset,
    absl::Time end_time, location::nearby::proto::connections::Medium medium) {
  LOG(INFO) << FormatPayloadTransferSummary(
      is_incoming, endpoint_id, payload_header, total_bytes, start_offset,
      end_time - start_time, medium);
}

}  // namespace nearby::connections
