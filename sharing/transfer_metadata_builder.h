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

#ifndef THIRD_PARTY_NEARBY_SHARING_TRANSFER_METADATA_BUILDER_H_
#define THIRD_PARTY_NEARBY_SHARING_TRANSFER_METADATA_BUILDER_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "sharing/transfer_metadata.h"

namespace nearby {
namespace sharing {

class TransferMetadataBuilder {
 public:
  static TransferMetadataBuilder Clone(const TransferMetadata& metadata);

  TransferMetadataBuilder();
  TransferMetadataBuilder(TransferMetadataBuilder&&);
  TransferMetadataBuilder& operator=(TransferMetadataBuilder&&);
  ~TransferMetadataBuilder();

  TransferMetadataBuilder& set_is_original(bool is_original);

  TransferMetadataBuilder& set_progress(double progress);

  TransferMetadataBuilder& set_status(TransferMetadata::Status status);

  TransferMetadataBuilder& set_token(std::optional<std::string> token);

  TransferMetadataBuilder& set_is_self_share(bool is_self_share);

  TransferMetadataBuilder& set_transferred_bytes(uint64_t transferred_bytes);

  TransferMetadataBuilder& set_transfer_speed(uint64_t transfer_speed);

  TransferMetadataBuilder& set_estimated_time_remaining(
      uint64_t estimated_time_remaining);

  TransferMetadataBuilder& set_total_attachments_count(
      int total_attachments_count);

  TransferMetadataBuilder& set_transferred_attachments_count(
      int transferred_attachments_count);

  TransferMetadataBuilder& set_in_progress_attachment_id(
      std::optional<int64_t> in_progress_attachment_id);

  TransferMetadataBuilder& set_in_progress_attachment_transferred_bytes(
      std::optional<uint64_t> in_progress_attachment_transferred_bytes);

  TransferMetadataBuilder& set_in_progress_attachment_total_bytes(
      std::optional<uint64_t> in_progress_attachment_total_bytes);

  TransferMetadata build() const;

 private:
  bool is_original_ = false;
  double progress_ = 0;
  TransferMetadata::Status status_ = TransferMetadata::Status::kInProgress;
  std::optional<std::string> token_;
  bool is_self_share_ = false;
  uint64_t transferred_bytes_ = 0;
  uint64_t transfer_speed_ = 0;
  uint64_t estimated_time_remaining_ = 0;
  int total_attachments_count_ = 0;
  int transferred_attachments_count_ = 0;
  std::optional<int64_t> in_progress_attachment_id_ = std::nullopt;
  std::optional<uint64_t> in_progress_attachment_transferred_bytes_ =
      std::nullopt;
  std::optional<uint64_t> in_progress_attachment_total_bytes_ = std::nullopt;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_TRANSFER_METADATA_BUILDER_H_
