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

#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/time/clock.h"
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

namespace nearby {
namespace sharing {

PayloadTracker::PayloadTracker(
    Clock* clock, int64_t share_target_id,
    const AttachmentContainer& container,
    const absl::flat_hash_map<int64_t, int64_t>& attachment_payload_map,
    std::function<void(int64_t, TransferMetadata)> update_callback)
    : clock_(clock),
      share_target_id_(share_target_id),
      update_callback_(std::move(update_callback)) {
  total_transfer_size_ = 0;
  confirmed_transfer_size_ = 0;

  for (const auto& file : container.GetFileAttachments()) {
    auto it = attachment_payload_map.find(file.id());
    if (it == attachment_payload_map.end()) {
      NL_LOG(WARNING)
          << __func__
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
      NL_LOG(WARNING)
          << __func__
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
      NL_LOG(WARNING) << __func__
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
    std::unique_ptr<PayloadTransferUpdate> update,
    std::optional<Medium> upgraded_medium) {
  auto it = payload_state_.find(update->payload_id);
  if (it == payload_state_.end()) return;

  // For metrics.
  if (!first_update_timestamp_.has_value()) {
    first_update_timestamp_ = absl::Now();
    num_first_update_bytes_ = update->bytes_transferred;
  }
  if (upgraded_medium.has_value()) {
    last_upgraded_medium_ = upgraded_medium;
  }

  if (it->second.status != update->status) {
    it->second.status = update->status;

    NL_VLOG(1) << __func__ << ": Payload id " << update->payload_id
               << " had status change: " << update->status;
  }

  if (it->second.status == PayloadStatus::kSuccess) {
    NL_LOG(INFO) << __func__ << ": Completed transfer of payload " << it->first
                 << " with attachment id " << it->second.attachment_id;
    transferred_attachments_count_++;
    confirmed_transfer_size_ += update->bytes_transferred;
  }

  // The number of bytes transferred should never go down. That said, some
  // status updates like cancellation might send a value of 0. In that case, we
  // retain the last known value for use in metrics.
  if (update->bytes_transferred > it->second.amount_transferred) {
    it->second.amount_transferred = update->bytes_transferred;
  }

  // Handle in progress attachment.
  if (!in_progress_payload_id_.has_value() ||
      *in_progress_payload_id_ != update->payload_id) {
    in_progress_payload_id_ = update->payload_id;
  }

  OnTransferUpdate(it->second);
}

void PayloadTracker::OnTransferUpdate(const State& state) {
  if (IsComplete()) {
    NL_VLOG(1) << __func__ << ": All payloads are complete.";
    update_callback_(
        share_target_id_,
        TransferMetadataBuilder()
            .set_status(TransferMetadata::Status::kComplete)
            .set_progress(100)
            .set_total_attachments_count(payload_state_.size())
            .set_transferred_attachments_count(transferred_attachments_count_)
            .build());
    return;
  }

  if (IsCancelled(state)) {
    NL_VLOG(1) << __func__ << ": Payloads cancelled.";
    update_callback_(
        share_target_id_,
        TransferMetadataBuilder()
            .set_status(TransferMetadata::Status::kCancelled)
            .set_total_attachments_count(payload_state_.size())
            .set_transferred_attachments_count(transferred_attachments_count_)
            .build());
    return;
  }

  if (HasFailed(state)) {
    NL_VLOG(1) << __func__ << ": Payloads failed.";
    update_callback_(
        share_target_id_,
        TransferMetadataBuilder()
            .set_status(TransferMetadata::Status::kFailed)
            .set_total_attachments_count(payload_state_.size())
            .set_transferred_attachments_count(transferred_attachments_count_)
            .build());
    return;
  }

  double percent = CalculateProgressPercent(state);
  int current_progress = static_cast<int>(percent);
  absl::Time current_time = clock_->Now();
  uint64_t current_transferred_size = GetTotalTransferred(state);

  if (current_progress == last_update_progress_ &&
      (current_time - last_update_timestamp_) < kMinProgressUpdateFrequency &&
      state.status != PayloadStatus::kSuccess) {
    return;
  }

  // Update transfer speed approximately every `kTransferSpeedUpdateInterval`
  // second.
  if (current_speed_ == 0 ||
      current_time - last_transfer_speed_update_timestamp_ >
          absl::Seconds(kTransferSpeedUpdateInterval)) {
    current_speed_ = (current_transferred_size - last_transferred_size_) /
                     absl::ToDoubleSeconds(
                         current_time - last_transfer_speed_update_timestamp_);

    // Use current speed for the ETA calculation for the first
    // `kEstimatedTimeRemainingUpdateInterval` seconds to avoid getting stuck at
    // showing 24+ hours left.
    if ((first_window_ == true) &&
        (current_time - last_eta_update_timestamp_ <
         absl::Seconds(kEstimatedTimeRemainingUpdateInterval))) {
      estimated_time_remaining_ =
          (total_transfer_size_ - current_transferred_size) /
          (current_speed_ + std::numeric_limits<uint64_t>::min());
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
    estimated_time_remaining_ =
        (total_transfer_size_ - current_transferred_size) /
        (average_speed + std::numeric_limits<uint64_t>::min());
    last_eta_update_timestamp_ = current_time;
    rolling_window_speed_bucket_ = 0.0;
  }

  last_update_progress_ = current_progress;
  last_update_timestamp_ = current_time;

  update_callback_(
      share_target_id_,
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kInProgress)
          .set_progress(percent)
          .set_transferred_bytes(current_transferred_size)
          .set_transfer_speed(static_cast<uint64_t>(current_speed_))
          .set_estimated_time_remaining(
              static_cast<uint64_t>(estimated_time_remaining_))
          .set_total_attachments_count(payload_state_.size())
          .set_transferred_attachments_count(transferred_attachments_count_)
          .set_in_progress_attachment_id(state.attachment_id)
          .set_in_progress_attachment_total_bytes(state.total_size)
          .set_in_progress_attachment_transferred_bytes(
              state.amount_transferred)
          .build());
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
    NL_LOG(WARNING) << __func__ << ": Total attachment size is 0";
    return 100.0;
  }

  return (100.0 * GetTotalTransferred(state)) / total_transfer_size_;
}

}  // namespace sharing
}  // namespace nearby
