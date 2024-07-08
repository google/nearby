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

#ifndef THIRD_PARTY_NEARBY_SHARING_PAYLOAD_TRACKER_H_
#define THIRD_PARTY_NEARBY_SHARING_PAYLOAD_TRACKER_H_

#include <stddef.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>

#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "internal/platform/clock.h"
#include "sharing/attachment_container.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/transfer_metadata.h"

namespace nearby {
namespace sharing {

// Listens for incoming or outgoing transfer updates from Nearby Connections and
// forwards the transfer progress to the |update_callback|.
class PayloadTracker : public NearbyConnectionsManager::PayloadStatusListener {
 public:
  PayloadTracker(
      Clock* clock, int64_t share_target_id,
      const AttachmentContainer& container,
      const absl::flat_hash_map<int64_t, int64_t>& attachment_payload_map,
      std::function<void(int64_t, TransferMetadata)> update_callback);
  ~PayloadTracker() override;

  // NearbyConnectionsManager::PayloadStatusListener:
  void OnStatusUpdate(std::unique_ptr<PayloadTransferUpdate> update,
                      std::optional<Medium> upgraded_medium) override;

 private:
  struct State {
    explicit State(int64_t attachment_id, int64_t total_size)
        : attachment_id(attachment_id), total_size(total_size) {}
    ~State() = default;

    int64_t attachment_id = 0;
    uint64_t amount_transferred = 0;
    const uint64_t total_size;
    PayloadStatus status = PayloadStatus::kInProgress;
  };

  void OnTransferUpdate(const State& state);

  bool IsComplete() const;
  bool IsCancelled(const State& state) const;
  bool HasFailed(const State& state) const;

  uint64_t GetTotalTransferred(const State& state) const;
  double CalculateProgressPercent(const State& state) const;

  Clock* const clock_;
  const int64_t share_target_id_;
  std::function<void(int64_t, TransferMetadata)> update_callback_;

  // Map of payload id to state of payload.
  std::map<int64_t, State> payload_state_;

  // Tracks in progress payload.
  std::optional<int64_t> in_progress_payload_id_ = std::nullopt;

  uint64_t total_transfer_size_;
  uint64_t confirmed_transfer_size_;

  int last_update_progress_ = 0;
  absl::Time last_update_timestamp_;  // progress percentage
  absl::Time last_transfer_speed_update_timestamp_;
  absl::Time last_eta_update_timestamp_;
  uint64_t last_transferred_size_ = 0;

  double current_speed_ = 0.0;
  double rolling_window_speed_bucket_ = 0.0;
  double estimated_time_remaining_ = 0.0;
  bool first_window_ = true;

  // Tracks transferred attachments count.
  int transferred_attachments_count_ = 0;

  // For metrics.
  size_t num_text_attachments_ = 0;
  size_t num_file_attachments_ = 0;
  size_t num_wifi_credentials_attachments_ = 0;
  uint64_t num_first_update_bytes_ = 0;
  std::optional<absl::Time> first_update_timestamp_;
  std::optional<Medium> last_upgraded_medium_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_PAYLOAD_TRACKER_H_
