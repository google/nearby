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

#ifndef THIRD_PARTY_NEARBY_SHARING_TRANSFER_METADATA_H_
#define THIRD_PARTY_NEARBY_SHARING_TRANSFER_METADATA_H_

#include <stdint.h>

#include <optional>
#include <string>

namespace nearby {
namespace sharing {

// Metadata about an ongoing transfer. Wraps transient data like status and
// progress. This is used to refresh the UI with error messages and show
// notifications so additions should be explicitly handled on the frontend.
class TransferMetadata {
 public:
  // LINT.IfChange()
  enum class Status {
    kUnknown,
    kConnecting,
    kAwaitingLocalConfirmation,
    kAwaitingRemoteAcceptance,
    kInProgress,
    kComplete,
    kFailed,
    kRejected,
    kCancelled,
    kTimedOut,
    kMediaUnavailable,
    kNotEnoughSpace,
    kUnsupportedAttachmentType,
    kDeviceAuthenticationFailed,
    kIncompletePayloads,
    kMaxValue = kIncompletePayloads
  };
  // LINT.ThenChange(//depot/google3/location/nearby/cpp/sharing/clients/dart/platform/lib/types/transfer_status.dart)

  static bool IsFinalStatus(Status status);
  static std::string StatusToString(TransferMetadata::Status status);

  TransferMetadata(
      Status status, float progress, std::optional<std::string> token,
      bool is_original, bool is_final_status, bool is_self_share,
      uint64_t transferred_bytes, uint64_t transfer_speed,
      uint64_t estimated_time_remaining, int total_attachments_count,
      int transferred_attachments_count,
      std::optional<int64_t> in_progress_attachment_id,
      std::optional<uint64_t> in_progress_attachment_transferred_bytes,
      std::optional<uint64_t> in_progress_attachment_total_bytes);
  ~TransferMetadata();
  TransferMetadata(const TransferMetadata&);
  TransferMetadata& operator=(const TransferMetadata&);

  Status status() const { return status_; }

  // Returns transfer progress as percentage.
  float progress() const { return progress_; }

  // Represents the UKey2 token from Nearby Connection. absl::nullopt if no
  // UKey2 comparison is needed for this transfer.
  const std::optional<std::string>& token() const { return token_; }

  // True if this |TransferMetadata| has not been seen.
  bool is_original() const { return is_original_; }

  // True if this |TransferMetadata| is the last status for this transfer.
  bool is_final_status() const { return is_final_status_; }

  // True if this |TransferMetadata| is for self share.
  bool is_self_share() const { return is_self_share_; }

  // Returns transferred attachment size in bytes.
  uint64_t transferred_bytes() const { return transferred_bytes_; }

  // Returns transfer speed in bytes per second.
  uint64_t transfer_speed() const { return transfer_speed_; }

  // Returns estimated time remaining in seconds.
  uint64_t estimated_time_remaining() const {
    return estimated_time_remaining_;
  }

  // Dumps this |TransferMetadata| to a summary string for logging purposes.
  std::string ToString() const;

  // Total attachments count in this sharing.
  int total_attachments_count() const { return total_attachments_count_; }

  // Completed attachment transfers in this sharing.
  int transferred_attachments_count() const {
    return transferred_attachments_count_;
  }

  std::optional<int64_t> in_progress_attachment_id() const {
    return in_progress_attachment_id_;
  }

  std::optional<uint64_t> in_progress_attachment_transferred_bytes() const {
    return in_progress_attachment_transferred_bytes_;
  }

  std::optional<uint64_t> in_progress_attachment_total_bytes() const {
    return in_progress_attachment_total_bytes_;
  }

 private:
  Status status_;
  float progress_;
  std::optional<std::string> token_;
  bool is_original_;
  bool is_final_status_;
  bool is_self_share_;
  uint64_t transferred_bytes_;
  uint64_t transfer_speed_;
  uint64_t estimated_time_remaining_;
  int total_attachments_count_;
  int transferred_attachments_count_;
  std::optional<int64_t> in_progress_attachment_id_;
  std::optional<uint64_t> in_progress_attachment_transferred_bytes_;
  std::optional<uint64_t> in_progress_attachment_total_bytes_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_TRANSFER_METADATA_H_
