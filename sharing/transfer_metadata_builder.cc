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

#include "sharing/transfer_metadata_builder.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "sharing/transfer_metadata.h"

namespace nearby {
namespace sharing {
// static
TransferMetadataBuilder TransferMetadataBuilder::Clone(
    const TransferMetadata& metadata) {
  TransferMetadataBuilder builder;
  builder.is_original_ = metadata.is_original();
  builder.progress_ = metadata.progress();
  builder.status_ = metadata.status();
  builder.token_ = metadata.token();
  builder.is_self_share_ = metadata.is_self_share();
  builder.transferred_bytes_ = metadata.transferred_bytes();
  builder.transfer_speed_ = metadata.transfer_speed();
  builder.estimated_time_remaining_ = metadata.estimated_time_remaining();
  builder.total_attachments_count_ = metadata.total_attachments_count();
  builder.transferred_attachments_count_ =
      metadata.transferred_attachments_count();
  builder.in_progress_attachment_id_ = metadata.in_progress_attachment_id();
  builder.in_progress_attachment_transferred_bytes_ =
      metadata.in_progress_attachment_transferred_bytes();
  return builder;
}

TransferMetadataBuilder::TransferMetadataBuilder() = default;

TransferMetadataBuilder::TransferMetadataBuilder(TransferMetadataBuilder&&) =
    default;

TransferMetadataBuilder& TransferMetadataBuilder::operator=(
    TransferMetadataBuilder&&) = default;

TransferMetadataBuilder::~TransferMetadataBuilder() = default;

TransferMetadataBuilder& TransferMetadataBuilder::set_is_original(
    bool is_original) {
  is_original_ = is_original;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_progress(
    double progress) {
  progress_ = progress;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_status(
    TransferMetadata::Status status) {
  status_ = status;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_token(
    std::optional<std::string> token) {
  token_ = std::move(token);
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_is_self_share(
    bool is_self_share) {
  is_self_share_ = is_self_share;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_transferred_bytes(
    uint64_t bytes) {
  transferred_bytes_ = bytes;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_transfer_speed(
    uint64_t speed) {
  transfer_speed_ = speed;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_estimated_time_remaining(
    uint64_t estimated_time_remaining) {
  estimated_time_remaining_ = estimated_time_remaining;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_total_attachments_count(
    int total_attachments_count) {
  total_attachments_count_ = total_attachments_count;
  return *this;
}

TransferMetadataBuilder&
TransferMetadataBuilder::set_transferred_attachments_count(
    int transferred_attachments_count) {
  transferred_attachments_count_ = transferred_attachments_count;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_in_progress_attachment_id(
    std::optional<int64_t> in_progress_attachment_id) {
  in_progress_attachment_id_ = in_progress_attachment_id;
  return *this;
}

TransferMetadataBuilder&
TransferMetadataBuilder::set_in_progress_attachment_transferred_bytes(
    std::optional<uint64_t> in_progress_attachment_transferred_bytes) {
  in_progress_attachment_transferred_bytes_ =
      in_progress_attachment_transferred_bytes;
  return *this;
}

TransferMetadataBuilder&
TransferMetadataBuilder::set_in_progress_attachment_total_bytes(
    std::optional<uint64_t> in_progress_attachment_total_bytes) {
  in_progress_attachment_total_bytes_ = in_progress_attachment_total_bytes;
  return *this;
}

TransferMetadata TransferMetadataBuilder::build() const {
  return TransferMetadata(
      status_, progress_, token_, is_original_,
      TransferMetadata::IsFinalStatus(status_), is_self_share_,
      transferred_bytes_, transfer_speed_, estimated_time_remaining_,
      total_attachments_count_, transferred_attachments_count_,
      in_progress_attachment_id_, in_progress_attachment_transferred_bytes_,
      in_progress_attachment_total_bytes_);
}

}  // namespace sharing
}  // namespace nearby
