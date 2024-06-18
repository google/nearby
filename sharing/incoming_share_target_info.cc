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

#include "sharing/incoming_share_target_info.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "sharing/attachment_container.h"
#include "sharing/constants.h"
#include "sharing/file_attachment.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/share_target_info.h"
#include "sharing/text_attachment.h"
#include "sharing/transfer_metadata.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby::sharing {
namespace {

using ::nearby::sharing::service::proto::IntroductionFrame;

}  // namespace

IncomingShareTargetInfo::IncomingShareTargetInfo(
    std::string endpoint_id, const ShareTarget& share_target,
    std::function<void(const IncomingShareTargetInfo&, const TransferMetadata&)>
        transfer_update_callback)
    : ShareTargetInfo(std::move(endpoint_id), share_target),
      transfer_update_callback_(std::move(transfer_update_callback)) {}

IncomingShareTargetInfo::IncomingShareTargetInfo(IncomingShareTargetInfo&&) =
    default;

IncomingShareTargetInfo& IncomingShareTargetInfo::operator=(
    IncomingShareTargetInfo&&) = default;

IncomingShareTargetInfo::~IncomingShareTargetInfo() = default;

void IncomingShareTargetInfo::InvokeTransferUpdateCallback(
    const TransferMetadata& metadata) {
  transfer_update_callback_(*this, metadata);
}

bool IncomingShareTargetInfo::OnNewConnection(NearbyConnection* connection) {
  set_disconnect_status(
      TransferMetadata::Status::kAwaitingRemoteAcceptanceFailed);
  return true;
}

std::optional<TransferMetadata::Status>
IncomingShareTargetInfo::ProcessIntroduction(
    const IntroductionFrame& introduction_frame) {
  int64_t file_size_sum = 0;
  AttachmentContainer& container = mutable_attachment_container();
  for (const auto& file : introduction_frame.file_metadata()) {
    if (file.size() <= 0) {
      NL_LOG(WARNING)
          << __func__
          << ": Ignore introduction, due to invalid attachment size";
      return TransferMetadata::Status::kUnsupportedAttachmentType;
    }

    NL_VLOG(1) << __func__ << ": Found file attachment: id=" << file.id()
               << ", type= " << file.type() << ", size=" << file.size()
               << ", payload_id=" << file.payload_id()
               << ", parent_folder=" << file.parent_folder()
               << ", mime_type=" << file.mime_type();
    container.AddFileAttachment(
        FileAttachment(file.id(), file.size(), file.name(), file.mime_type(),
                       file.type(), file.parent_folder()));
    SetAttachmentPayloadId(file.id(), file.payload_id());

    if (std::numeric_limits<int64_t>::max() - file.size() < file_size_sum) {
      NL_LOG(WARNING) << __func__
                      << ": Ignoring introduction, total file size overflowed "
                         "64 bit integer.";
      container.Clear();
      return TransferMetadata::Status::kNotEnoughSpace;
    }
    file_size_sum += file.size();
  }

  for (const auto& text : introduction_frame.text_metadata()) {
    if (text.size() <= 0) {
      NL_LOG(WARNING)
          << __func__
          << ": Ignore introduction, due to invalid attachment size";
      return TransferMetadata::Status::kUnsupportedAttachmentType;
    }

    NL_VLOG(1) << __func__ << ": Found text attachment: id=" << text.id()
               << ", type= " << text.type() << ", size=" << text.size()
               << ", payload_id=" << text.payload_id();
    container.AddTextAttachment(
        TextAttachment(text.id(), text.type(), text.text_title(), text.size()));
    SetAttachmentPayloadId(text.id(), text.payload_id());
  }

  if (kSupportReceivingWifiCredentials) {
    for (const auto& wifi_credentials :
         introduction_frame.wifi_credentials_metadata()) {
      NL_VLOG(1) << __func__ << ": Found WiFi credentials attachment: id="
                 << wifi_credentials.id()
                 << ", ssid= " << wifi_credentials.ssid()
                 << ", payload_id=" << wifi_credentials.payload_id();
      container.AddWifiCredentialsAttachment(WifiCredentialsAttachment(
          wifi_credentials.id(), wifi_credentials.ssid(),
          wifi_credentials.security_type()));
      SetAttachmentPayloadId(wifi_credentials.id(),
                             wifi_credentials.payload_id());
    }
  }

  if (!container.HasAttachments()) {
    NL_LOG(WARNING) << __func__
                    << ": No attachment is found for this share target. It can "
                       "be result of unrecognizable attachment type";
    return TransferMetadata::Status::kUnsupportedAttachmentType;
  }
  return std::nullopt;
}

}  // namespace nearby::sharing
