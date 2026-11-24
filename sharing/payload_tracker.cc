// Copyright 2022-2023 Google LLC
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

#include "sharing/payload_tracker.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "internal/platform/clock.h"
#include "sharing/attachment_container.h"
#include "sharing/constants.h"
#include "sharing/file_attachment.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/text_attachment.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"
#include "sharing/wifi_credentials_attachment.h"
#include "sharing/worker_queue.h"

namespace nearby {
namespace sharing {
namespace {

double CalculateEstimatedTimeRemainingSeconds(uint64_t total_transfer_size,
                                              uint64_t current_transferred_size,
                                              double speed_bytes_per_second) {
  if (total_transfer_size <= current_transferred_size ||
      !std::isfinite(speed_bytes_per_second) || speed_bytes_per_second <= 0.0) {
    return 0.0;
  }
  double remaining_seconds =
      static_cast<double>(total_transfer_size - current_transferred_size) /
      speed_bytes_per_second;
  if (!std::isfinite(remaining_seconds) || remaining_seconds <= 0.0) {
    return 0.0;
  }
  return remaining_seconds;
}

uint64_t ClampToUint64(double value) {
  if (!std::isfinite(value) || value <= 0.0) {
    return 0;
  }
  if (value >= std::ldexp(1.0, 64)) {
    return std::numeric_limits<uint64_t>::max();
  }
  return static_cast<uint64_t>(value);
}

uint64_t RoundSecondsToUint64(double seconds) {
  return ClampToUint64(std::round(seconds));
}

}  // namespace

PayloadTracker::PayloadTracker(
    Clock* clock, int64_t share_target_id, const AttachmentContainer& container,
    const absl::flat_hash_map<int64_t, int64_t>& attachment_payload_map,
    std::unique_ptr<WorkerQueue<std::unique_ptr<PayloadTransferUpdate>>>
        payload_queue)
    : clock_(clock),
      share_target_id_(share_target_id),
      payload_update_queue_(std::move(payload_queue)) {
  total_transfer_size_ = 0;
  confirmed_transfer_size_ = 0;

  for (const auto& file : container.GetFileAttachments()) {
    auto it = attachment_payload_map.find(file.id());
    if (it == attachment_payload_map.end()) {
      LOG(WARNING) << __func__
                   << ": Failed to retrieve payload for file attachment id - "
                   << file.id();
      continue;
    }

    payload_state_.emplace(it->second, State(file.id(), file.size()));
    ++num_file_attachments_;
    total_transfer_size_ += file.size();
  }

  for (const auto& text : container.GetTextAttachments()) {
    auto it = attachment_payload_map.find(text.id());
    if (it == attachment_payload_map.end()) {
      LOG(WARNING) << __func__
                   << ": Failed to retrieve payload for text attachment id - "
                   << text.id();
      continue;
    }

    payload_state_.emplace(it->second, State(text.id(), text.size()));
    ++num_text_attachments_;
    total_transfer_size_ += text.size();
  }

  for (const auto& wifi_credentials :
       container.GetWifiCredentialsAttachments()) {
    auto it = attachment_payload_map.find(wifi_credentials.id());
    if (it == attachment_payload_map.end()) {
      LOG(WARNING) << __func__
                   << ": Failed to retrieve payload for WiFi credentials "
                      "attachment id - "
                   << wifi_credentials.id();
      continue;
    }

    payload_state_.emplace(
        it->second, State(wifi_credentials.id(), wifi_credentials.size()));
    ++num_wifi_credentials_attachments_;
    total_transfer_size_ += wifi_credentials.size();
  }
}

PayloadTracker::~PayloadTracker() = default;

void PayloadTracker::OnStatusUpdate(
    std::unique_ptr<PayloadTransferUpdate> update) {
  if (payload_state_.find(update->payload_id) == payload_state_.end()) {
    LOG(ERROR) << "Got transfer update for untracked payload: "
               << update->payload_id;
    return;
  }
  payload_update_queue_->Queue(std::move(update));
}

std::optional<TransferMetadataBuilder> PayloadTracker::ProcessPayloadUpdate(
    std::unique_ptr<PayloadTransferUpdate> update) {
  auto it = payload_state_.find(update->payload_id);
  if (it == payload_state_.end()) {
    return std::nullopt;
  }
  State& state = it->second;
  if (state.status != update->status) {
    state.status = update->status;

    VLOG(1) << __func__ << ": Payload id " << update->payload_id
            << " had status change: " << update->status;
  }

  if (state.status == PayloadStatus::kSuccess) {
    LOG(INFO) << __func__ << ": Completed transfer of payload "
              << update->payload_id << " with attachment id "
              << state.attachment_id;
    transferred_attachments_count_++;
    confirmed_transfer_size_ += update->bytes_transferred;
  }

  // The number of bytes transferred should never go down. That said, some
  // status updates like cancellation might send a value of 0. In that case, we
  // retain the last known value for use in metrics.
  if (update->bytes_transferred > state.amount_transferred) {
    state.amount_transferred = update->bytes_transferred;
  }

  return OnTransferUpdate(state);
}

std::optional<TransferMetadataBuilder> PayloadTracker::OnTransferUpdate(
    const State& state) {
  if (IsComplete()) {
    VLOG(1) << __func__ << ": All payloads are complete.";
    return std::move(TransferMetadataBuilder()
        .set_status(TransferMetadata::Status::kComplete)
        .set_progress(100)
        .set_total_attachments_count(payload_state_.size())
        .set_transferred_attachments_count(transferred_attachments_count_));
  }

  if (IsCancelled(state)) {
    VLOG(1) << __func__ << ": Payloads cancelled.";
    return std::move(TransferMetadataBuilder()
        .set_status(TransferMetadata::Status::kCancelled)
        .set_total_attachments_count(payload_state_.size())
        .set_transferred_attachments_count(transferred_attachments_count_));
  }

  if (HasFailed(state)) {
    VLOG(1) << __func__ << ": Payloads failed.";
    return std::move(TransferMetadataBuilder()
        .set_status(TransferMetadata::Status::kFailed)
        .set_total_attachments_count(payload_state_.size())
        .set_transferred_attachments_count(transferred_attachments_count_));
  }

  double percent = CalculateProgressPercent(state);
  int current_progress = static_cast<int>(percent);
  absl::Time current_time = clock_->Now();
  uint64_t current_transferred_size = GetTotalTransferred(state);

  if (current_progress == last_update_progress_ &&
      state.status != PayloadStatus::kSuccess) {
    return std::nullopt;
  }

  // Update transfer speed approximately every `kTransferSpeedUpdateInterval`
  // second.
  if (current_speed_ == 0 ||
      current_time - last_transfer_speed_update_timestamp_ >
          absl::Seconds(kTransferSpeedUpdateInterval)) {
    double elapsed_seconds = absl::ToDoubleSeconds(
        current_time - last_transfer_speed_update_timestamp_);
    current_speed_ = (elapsed_seconds > 0.0 &&
                      current_transferred_size >= last_transferred_size_)
                         ? static_cast<double>(current_transferred_size -
                                               last_transferred_size_) /
                               elapsed_seconds
                         : 0.0;

    // Use current speed for the ETA calculation for the first
    // `kEstimatedTimeRemainingUpdateInterval` seconds to avoid getting stuck at
    // showing 24+ hours left.
    if ((first_window_ == true) &&
        (current_time - last_eta_update_timestamp_ <
         absl::Seconds(kEstimatedTimeRemainingUpdateInterval))) {
      estimated_time_remaining_ = CalculateEstimatedTimeRemainingSeconds(
          total_transfer_size_, current_transferred_size, current_speed_);
      first_window_ = false;
    }

    rolling_window_speed_bucket_ += current_speed_;
    last_transferred_size_ = current_transferred_size;
    last_transfer_speed_update_timestamp_ = current_time;
  }

  // Update estimated time remaining approximately every
  // `kEstimatedTimeRemainingUpdateInterval` seconds.
  if (current_time - last_eta_update_timestamp_ >
      absl::Seconds(kEstimatedTimeRemainingUpdateInterval)) {
    double average_speed =
        rolling_window_speed_bucket_ / kEstimatedTimeRemainingUpdateInterval;
    estimated_time_remaining_ = CalculateEstimatedTimeRemainingSeconds(
        total_transfer_size_, current_transferred_size, average_speed);
    last_eta_update_timestamp_ = current_time;
    rolling_window_speed_bucket_ = 0.0;
  }

  last_update_progress_ = current_progress;

  return std::move(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .set_progress(percent)
          .set_transferred_bytes(current_transferred_size)
          .set_transfer_speed(ClampToUint64(current_speed_))
          .set_estimated_time_remaining(
              RoundSecondsToUint64(estimated_time_remaining_))
          .set_total_attachments_count(payload_state_.size())
          .set_transferred_attachments_count(transferred_attachments_count_)
          .set_in_progress_attachment_id(state.attachment_id)
          .set_in_progress_attachment_total_bytes(state.total_size)
          .set_in_progress_attachment_transferred_bytes(
              state.amount_transferred));
}

bool PayloadTracker::IsComplete() const {
  return transferred_attachments_count_ == payload_state_.size();
}

bool PayloadTracker::IsCancelled(const State& state) const {
  return state.status == PayloadStatus::kCanceled;
}

bool PayloadTracker::HasFailed(const State& state) const {
  return state.status == PayloadStatus::kFailure;
}

uint64_t PayloadTracker::GetTotalTransferred(const State& state) const {
  if (state.status == PayloadStatus::kSuccess) {
    return confirmed_transfer_size_;
  }
  return confirmed_transfer_size_ + state.amount_transferred;
}

double PayloadTracker::CalculateProgressPercent(const State& state) const {
  if (!total_transfer_size_) {
    LOG(WARNING) << __func__ << ": Total attachment size is 0";
    return 100.0;
  }

  return (100.0 * GetTotalTransferred(state)) / total_transfer_size_;
}

}  // namespace sharing
}  // namespace nearby
