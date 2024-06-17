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

#include "sharing/outgoing_share_target_info.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>  // NOLINT
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "sharing/attachment_container.h"
#include "sharing/file_attachment.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/nearby_file_handler.h"
#include "sharing/share_target.h"
#include "sharing/share_target_info.h"
#include "sharing/text_attachment.h"
#include "sharing/transfer_metadata.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby::sharing {

OutgoingShareTargetInfo::OutgoingShareTargetInfo(
    std::string endpoint_id, const ShareTarget& share_target,
    std::function<void(OutgoingShareTargetInfo&, const TransferMetadata&)>
        transfer_update_callback)
    : ShareTargetInfo(std::move(endpoint_id), share_target),
      transfer_update_callback_(std::move(transfer_update_callback)) {}

OutgoingShareTargetInfo::OutgoingShareTargetInfo(OutgoingShareTargetInfo&&) =
    default;

OutgoingShareTargetInfo& OutgoingShareTargetInfo::operator=(
    OutgoingShareTargetInfo&&) = default;

OutgoingShareTargetInfo::~OutgoingShareTargetInfo() = default;

void OutgoingShareTargetInfo::InvokeTransferUpdateCallback(
    const TransferMetadata& metadata) {
  transfer_update_callback_(*this, metadata);
}

bool OutgoingShareTargetInfo::OnNewConnection(NearbyConnection* connection) {
  if (!connection) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to initiate connection to share target "
                    << share_target().id;
    if (connection_layer_status_ == Status::kTimeout) {
      set_disconnect_status(TransferMetadata::Status::kTimedOut);
      connection_layer_status_ = Status::kUnknown;
    } else {
      set_disconnect_status(
          TransferMetadata::Status::kFailedToInitiateOutgoingConnection);
    }
    return false;
  }
  set_disconnect_status(TransferMetadata::Status::kUnexpectedDisconnection);
  return true;
}

std::vector<std::filesystem::path> OutgoingShareTargetInfo::GetFilePaths()
    const {
  std::vector<std::filesystem::path> file_paths;
  file_paths.reserve(attachment_container().GetFileAttachments().size());
  for (const FileAttachment& file_attachment :
       attachment_container().GetFileAttachments()) {
    // All file attachments must have a file path.
    // That is verified in SendAttachments().
    file_paths.push_back(*file_attachment.file_path());
  }
  return file_paths;
}

void OutgoingShareTargetInfo::CreateTextPayloads() {
  const std::vector<TextAttachment> attachments =
      attachment_container().GetTextAttachments();
  if (attachments.empty()) {
    return;
  }
  text_payloads_.clear();
  text_payloads_.reserve(attachments.size());
  for (const TextAttachment& attachment : attachments) {
    absl::string_view body = attachment.text_body();
    std::vector<uint8_t> bytes(body.begin(), body.end());
    text_payloads_.emplace_back(bytes);
    SetAttachmentPayloadId(attachment.id(), text_payloads_.back().id);
  }
}

void OutgoingShareTargetInfo::CreateWifiCredentialsPayloads() {
  const std::vector<WifiCredentialsAttachment> attachments =
      attachment_container().GetWifiCredentialsAttachments();
  if (attachments.empty()) {
    return;
  }
  wifi_credentials_payloads_.clear();
  wifi_credentials_payloads_.reserve(attachments.size());
  for (const WifiCredentialsAttachment& attachment : attachments) {
    nearby::sharing::service::proto::WifiCredentials wifi_credentials;
    wifi_credentials.set_password(std::string(attachment.password()));
    wifi_credentials.set_hidden_ssid(attachment.is_hidden());

    std::vector<uint8_t> bytes(wifi_credentials.ByteSizeLong());
    wifi_credentials.SerializeToArray(bytes.data(),
                                      wifi_credentials.ByteSizeLong());
    wifi_credentials_payloads_.emplace_back(bytes);
    SetAttachmentPayloadId(attachment.id(),
                           wifi_credentials_payloads_.back().id);
  }
}

bool OutgoingShareTargetInfo::CreateFilePayloads(
    const std::vector<NearbyFileHandler::FileInfo>& files) {
  AttachmentContainer& container = mutable_attachment_container();
  if (files.size() != container.GetFileAttachments().size()) {
    return false;
  }
  if (files.empty()) {
    return true;
  }
  file_payloads_.clear();
  file_payloads_.reserve(files.size());

  for (size_t i = 0; i < files.size(); ++i) {
    const NearbyFileHandler::FileInfo& file_info = files[i];
    FileAttachment& attachment = container.GetMutableFileAttachment(i);
    attachment.set_size(file_info.size);
    InputFile input_file;
    input_file.path = file_info.file_path;
    Payload payload(input_file, attachment.parent_folder());
    payload.content.file_payload.size = file_info.size;
    file_payloads_.push_back(std::move(payload));
    SetAttachmentPayloadId(attachment.id(), file_payloads_.back().id);
  }
  return true;
}

std::vector<Payload> OutgoingShareTargetInfo::ExtractTextPayloads() {
  return std::move(text_payloads_);
}

std::vector<Payload> OutgoingShareTargetInfo::ExtractFilePayloads() {
  return std::move(file_payloads_);
}

std::optional<Payload> OutgoingShareTargetInfo::ExtractNextPayload() {
  if (!text_payloads_.empty()) {
    Payload payload = text_payloads_.back();
    text_payloads_.pop_back();
    return payload;
  }

  if (!file_payloads_.empty()) {
    Payload payload = file_payloads_.back();
    file_payloads_.pop_back();
    return payload;
  }

  if (!wifi_credentials_payloads_.empty()) {
    Payload payload = wifi_credentials_payloads_.back();
    wifi_credentials_payloads_.pop_back();
    return payload;
  }

  return std::nullopt;
}

}  // namespace nearby::sharing
