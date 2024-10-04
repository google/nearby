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

#include "sharing/transfer_metadata.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

namespace nearby {
namespace sharing {

// static
bool TransferMetadata::IsFinalStatus(Status status) {
  switch (status) {
    case Status::kCancelled:
    case Status::kComplete:
    case Status::kFailed:
    case Status::kIncompletePayloads:
    case Status::kMediaUnavailable:
    case Status::kNotEnoughSpace:
    case Status::kDeviceAuthenticationFailed:
    case Status::kRejected:
    case Status::kTimedOut:
    case Status::kUnsupportedAttachmentType:
      return true;
    case Status::kAwaitingLocalConfirmation:
    case Status::kAwaitingRemoteAcceptance:
    case Status::kConnecting:
    case Status::kInProgress:
    case Status::kUnknown:
      return false;
  }
}

// static
// LINT.IfChange()
std::string TransferMetadata::StatusToString(Status status) {
  switch (status) {
    case Status::kConnecting:
      return "kConnecting";
    case Status::kUnknown:
      return "kUnknown";
    case Status::kAwaitingLocalConfirmation:
      return "kAwaitingLocalConfirmation";
    case Status::kAwaitingRemoteAcceptance:
      return "kAwaitingRemoteAcceptance";
    case Status::kInProgress:
      return "kInProgress";
    case Status::kComplete:
      return "kComplete";
    case Status::kFailed:
      return "kFailed";
    case Status::kRejected:
      return "kReject";
    case Status::kCancelled:
      return "kCancelled";
    case Status::kTimedOut:
      return "kTimedOut";
    case Status::kMediaUnavailable:
      return "kMediaUnavailable";
    case Status::kNotEnoughSpace:
      return "kNotEnoughSpace";
    case Status::kUnsupportedAttachmentType:
      return "kUnsupportedAttachmentType";
    case Status::kDeviceAuthenticationFailed:
      return "kDeviceAuthenticationFailed";
    case Status::kIncompletePayloads:
      return "kIncompletePayloads";
  }
}
// LINT.ThenChange(//depot/google3/location/nearby/cpp/sharing/clients/dart/platform/lib/types/transfer_status.dart)

TransferMetadata::TransferMetadata(
    Status status, float progress, std::optional<std::string> token,
    bool is_original, bool is_final_status, bool is_self_share,
    uint64_t transferred_bytes, uint64_t transfer_speed,
    uint64_t estimated_time_remaining, int total_attachments_count,
    int transferred_attachments_count,
    std::optional<int64_t> in_progress_attachment_id,
    std::optional<uint64_t> in_progress_attachment_transferred_bytes,
    std::optional<uint64_t> in_progress_attachment_total_bytes)
    : status_(status),
      progress_(progress),
      token_(std::move(token)),
      is_original_(is_original),
      is_final_status_(is_final_status),
      is_self_share_(is_self_share),
      transferred_bytes_(transferred_bytes),
      transfer_speed_(transfer_speed),
      estimated_time_remaining_(estimated_time_remaining),
      total_attachments_count_(total_attachments_count),
      transferred_attachments_count_(transferred_attachments_count),
      in_progress_attachment_id_(in_progress_attachment_id),
      in_progress_attachment_transferred_bytes_(
          in_progress_attachment_transferred_bytes),
      in_progress_attachment_total_bytes_(in_progress_attachment_total_bytes) {}

TransferMetadata::~TransferMetadata() = default;

TransferMetadata::TransferMetadata(const TransferMetadata&) = default;

TransferMetadata& TransferMetadata::operator=(const TransferMetadata&) =
    default;

std::string TransferMetadata::ToString() const {
  std::vector<std::string> fmt;

  fmt.push_back(absl::StrFormat("status: %s", StatusToString(status_)));
  fmt.push_back(absl::StrFormat("progress: %.2f", progress_));
  if (token_) {
    fmt.push_back(absl::StrFormat("token: %s", *token_));
  }
  fmt.push_back(absl::StrFormat("is_original: %d", is_original_));
  fmt.push_back(absl::StrFormat("is_final_status: %d", is_final_status_));
  fmt.push_back(absl::StrFormat("is_self_share: %d", is_self_share_));
  fmt.push_back(absl::StrFormat("transferred_bytes: %d", transferred_bytes_));
  fmt.push_back(absl::StrFormat("transfer_speed: %d", transfer_speed_));
  fmt.push_back(absl::StrFormat("estimated_time_remaining: %d",
                                estimated_time_remaining_));

  return absl::StrCat("TransferMetadata<", absl::StrJoin(fmt, ", "), ">");
}

}  // namespace sharing
}  // namespace nearby
