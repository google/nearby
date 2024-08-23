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

#include "sharing/incoming_share_session.h"

#include <cstdint>
#include <filesystem>  // NOLINT
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "internal/platform/clock.h"
#include "internal/platform/task_runner.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment_container.h"
#include "sharing/common/compatible_u8_string.h"
#include "sharing/constants.h"
#include "sharing/file_attachment.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/payload_tracker.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_session.h"
#include "sharing/share_target.h"
#include "sharing/text_attachment.h"
#include "sharing/thread_timer.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby::sharing {
namespace {

using ::location::nearby::proto::sharing::OSType;
using ::nearby::sharing::service::proto::ConnectionResponseFrame;
using ::nearby::sharing::service::proto::IntroductionFrame;
using ::location::nearby::proto::sharing::ResponseToIntroduction;
using ::nearby::sharing::service::proto::V1Frame;
using ::nearby::sharing::service::proto::WifiCredentials;

}  // namespace

IncomingShareSession::IncomingShareSession(
    TaskRunner& service_thread,
    analytics::AnalyticsRecorder& analytics_recorder, std::string endpoint_id,
    const ShareTarget& share_target,
    std::function<void(const IncomingShareSession&, const TransferMetadata&)>
        transfer_update_callback)
    : ShareSession(service_thread, analytics_recorder, std::move(endpoint_id),
                   share_target),
      transfer_update_callback_(std::move(transfer_update_callback)) {}

IncomingShareSession::IncomingShareSession(IncomingShareSession&&) = default;

IncomingShareSession::~IncomingShareSession() = default;

void IncomingShareSession::InvokeTransferUpdateCallback(
    const TransferMetadata& metadata) {
  transfer_update_callback_(*this, metadata);
}

bool IncomingShareSession::OnNewConnection(NearbyConnection* connection) {
  set_disconnect_status(TransferMetadata::Status::kFailed);
  return true;
}

std::optional<TransferMetadata::Status>
IncomingShareSession::ProcessIntroduction(
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

bool IncomingShareSession::ProcessKeyVerificationResult(
    PairedKeyVerificationRunner::PairedKeyVerificationResult result,
    OSType share_target_os_type,
    std::function<void(std::optional<IntroductionFrame>)>
        introduction_callback) {
  if (!HandleKeyVerificationResult(result, share_target_os_type)) {
    return false;
  }
  NL_LOG(INFO) << __func__ << ": Waiting for introduction from "
               << share_target().id;

  frames_reader()->ReadFrame(
      V1Frame::INTRODUCTION,
      [callback =
           std::move(introduction_callback)](std::optional<V1Frame> frame) {
        if (!frame.has_value()) {
          callback(std::nullopt);
        } else {
          callback(frame->introduction());
        }
      },
      kReadFramesTimeout);
  return true;
}

bool IncomingShareSession::ReadyForTransfer(
    std::function<void()> accept_timeout_callback,
    std::function<void(std::optional<V1Frame> frame)> frame_read_callback) {
  if (!IsConnected()) {
    NL_LOG(WARNING) << __func__ << ": out of order API call.";
    return false;
  }
  ready_for_accept_ = true;
  set_disconnect_status(TransferMetadata::Status::kFailed);

  mutual_acceptance_timeout_ = std::make_unique<ThreadTimer>(
      service_thread(), "incoming_mutual_acceptance_timeout",
      kReadResponseFrameTimeout, std::move(accept_timeout_callback));
  frames_reader()->ReadFrame(std::move(frame_read_callback));

  if (!self_share()) {
    TransferMetadataBuilder transfer_metadata_builder;
    transfer_metadata_builder.set_status(
        TransferMetadata::Status::kAwaitingLocalConfirmation);
    transfer_metadata_builder.set_token(token());

    UpdateTransferMetadata(transfer_metadata_builder.build());
    return false;
  }
  return true;
}

bool IncomingShareSession::AcceptTransfer(
    Clock* clock,
    std::function<void(int64_t, TransferMetadata)> update_callback) {
  if (!ready_for_accept_ || !IsConnected()) {
    NL_LOG(WARNING) << __func__ << ": out of order API call.";
    return false;
  }
  ready_for_accept_ = false;
  const absl::flat_hash_map<int64_t, int64_t>& payload_map =
      attachment_payload_map();
  set_payload_tracker(std::make_shared<PayloadTracker>(
      clock, share_target().id, attachment_container(), payload_map,
      std::move(update_callback)));

  // Register status listener for all payloads.
  for (auto it = payload_map.begin(); it != payload_map.end(); ++it) {
    NL_VLOG(1) << __func__
               << ": Started listening for progress on payload: " << it->second
               << " for attachment: " << it->first;

    connections_manager()->RegisterPayloadStatusListener(it->second,
                                                        payload_tracker());

    NL_VLOG(1) << __func__ << ": Accepted incoming files from share target - "
               << share_target().id;
  }
  WriteResponseFrame(ConnectionResponseFrame::ACCEPT);
  NL_VLOG(1) << __func__ << ": Successfully wrote response frame";
  // Log analytics event of responding to introduction.
  analytics_recorder().NewRespondToIntroduction(
      ResponseToIntroduction::ACCEPT_INTRODUCTION, session_id());

  UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingRemoteAcceptance)
          .set_token(token())
          .build());

  if (TryUpgradeBandwidth()) {
    // Upgrade bandwidth regardless of advertising visibility because either
    // the system or the user has verified the sender's identity; the
    // stable identifiers potentially exposed by performing a bandwidth
    // upgrade are no longer a concern.
    NL_LOG(INFO) << __func__ << ": Upgrade bandwidth when sending accept.";
  }
  // Log analytics event of starting to receive payloads.
  analytics_recorder().NewReceiveAttachmentsStart(session_id(),
                                                  attachment_container());
  return true;
}

bool IncomingShareSession::UpdateFilePayloadPaths() {
  AttachmentContainer& container = mutable_attachment_container();
  bool result = true;
  for (int i = 0; i < container.GetFileAttachments().size(); ++i) {
    FileAttachment& file = container.GetMutableFileAttachment(i);
    // Skip file if it already has file_path set.
    if (file.file_path().has_value()) {
      continue;
    }
    const auto it = attachment_payload_map().find(file.id());
    if (it == attachment_payload_map().end()) {
      NL_LOG(WARNING) << __func__ << ": No payload id found for file - "
                      << file.id();
      result = false;
      continue;
    }

    const Payload* incoming_payload =
        connections_manager()->GetIncomingPayload(it->second);
    if (!incoming_payload || !incoming_payload->content.is_file()) {
      NL_LOG(WARNING) << __func__ << ": No payload found for file - "
                      << file.id();
      result = false;
      continue;
    }

    auto file_path = incoming_payload->content.file_payload.file.path;
    NL_VLOG(1) << __func__ << ": Updated file_path="
               << GetCompatibleU8String(file_path.u8string());
    file.set_file_path(file_path);
  }
  return result;
}

bool IncomingShareSession::UpdatePayloadContents() {
  if (!UpdateFilePayloadPaths()) {
    return false;
  }
  AttachmentContainer& container = mutable_attachment_container();
  for (int i = 0; i < container.GetTextAttachments().size(); ++i) {
    TextAttachment& text = container.GetMutableTextAttachment(i);
    const auto it = attachment_payload_map().find(text.id());
    if (it == attachment_payload_map().end()) {
      // This should never happen unless IntroductionFrame has not been
      // processed.
      NL_LOG(WARNING) << __func__ << ": No payload id found for text - "
                      << text.id();
      return false;
    }
    const Payload* incoming_payload =
        connections_manager()->GetIncomingPayload(it->second);
    if (!incoming_payload || !incoming_payload->content.is_bytes()) {
      NL_LOG(WARNING) << __func__ << ": No payload found for text - "
                      << text.id();
      return false;
    }

    std::vector<uint8_t> bytes = incoming_payload->content.bytes_payload.bytes;
    if (bytes.empty()) {
      NL_LOG(WARNING)
          << __func__
          << ": Incoming bytes is empty for text payload with payload_id - "
          << it->second;
      return false;
    }

    std::string text_body(bytes.begin(), bytes.end());
    text.set_text_body(text_body);
  }

  for (int i = 0; i < container.GetWifiCredentialsAttachments().size(); ++i) {
    WifiCredentialsAttachment& wifi_credentials_attachment =
        container.GetMutableWifiCredentialsAttachment(i);
    const auto it =
        attachment_payload_map().find(wifi_credentials_attachment.id());
    if (it == attachment_payload_map().end()) {
      // This should never happen unless IntroductionFrame has not been
      // processed.
      NL_LOG(WARNING) << __func__
                      << ": No payload id found for WiFi credentials - "
                      << wifi_credentials_attachment.id();
      return false;
    }

    const Payload* incoming_payload =
        connections_manager()->GetIncomingPayload(it->second);
    if (!incoming_payload || !incoming_payload->content.is_bytes()) {
      NL_LOG(WARNING) << __func__
                      << ": No payload found for WiFi credentials - "
                      << wifi_credentials_attachment.id();
      return false;
    }

    std::vector<uint8_t> bytes = incoming_payload->content.bytes_payload.bytes;
    if (bytes.empty()) {
      NL_LOG(WARNING) << __func__
                      << ": Incoming bytes is empty for WiFi credentials "
                         "payload with payload_id - "
                      << it->second;
      return false;
    }

    WifiCredentials wifi_credentials;
    if (!wifi_credentials.ParseFromArray(bytes.data(), bytes.size())) {
      NL_LOG(WARNING) << __func__
                      << ": Incoming bytes is invalid for WiFi credentials "
                         "payload with payload_id - "
                      << it->second;
      return false;
    }

    wifi_credentials_attachment.set_password(wifi_credentials.password());
    wifi_credentials_attachment.set_is_hidden(wifi_credentials.hidden_ssid());
  }
  return true;
}

bool IncomingShareSession::FinalizePayloads() {
  if (!UpdatePayloadContents()) {
    mutable_attachment_container().ClearAttachments();
    return false;
  }
  return true;
}

std::vector<std::filesystem::path> IncomingShareSession::GetPayloadFilePaths()
    const {
  std::vector<std::filesystem::path> file_paths;
  const AttachmentContainer& container = attachment_container();
  const absl::flat_hash_map<int64_t, int64_t>& attachment_paylod_map =
      attachment_payload_map();
  for (const auto& file : container.GetFileAttachments()) {
    if (!file.file_path().has_value()) continue;
    auto file_path = *file.file_path();
    NL_VLOG(1) << __func__
               << ": file_path=" << GetCompatibleU8String(file_path.u8string());
    if (attachment_paylod_map.find(file.id()) == attachment_paylod_map.end()) {
      continue;
    }
    file_paths.push_back(file_path);
  }
  return file_paths;
}

bool IncomingShareSession::TryUpgradeBandwidth() {
  if (!bandwidth_upgrade_requested_ &&
      attachment_container().GetTotalAttachmentsSize() >=
          kAttachmentsSizeThresholdOverHighQualityMedium) {
    connections_manager()->UpgradeBandwidth(endpoint_id());
    bandwidth_upgrade_requested_ = true;
    return true;
  }
  return false;
}

void IncomingShareSession::SendFailureResponse(
    TransferMetadata::Status status) {
  // Send response to remote device.
  ConnectionResponseFrame::Status response_status;
  switch (status) {
    case TransferMetadata::Status::kNotEnoughSpace:
      response_status = ConnectionResponseFrame::NOT_ENOUGH_SPACE;
      break;

    case TransferMetadata::Status::kUnsupportedAttachmentType:
      response_status = ConnectionResponseFrame::UNSUPPORTED_ATTACHMENT_TYPE;
      break;

    case TransferMetadata::Status::kTimedOut:
      response_status = ConnectionResponseFrame::TIMED_OUT;
      break;

    default:
      response_status = ConnectionResponseFrame::UNKNOWN;
      break;
  }

  WriteResponseFrame(response_status);
  NL_DCHECK(TransferMetadata::IsFinalStatus(status))
      << "SendFailureResponse should only be called with a final status";
  UpdateTransferMetadata(
      TransferMetadataBuilder().set_status(status).build());
}

std::pair<bool, bool> IncomingShareSession::PayloadTransferUpdate(
    bool update_file_paths_in_progress, const TransferMetadata& metadata) {
  // Cancel acceptance timer when payload transfer update is received.
  // This mean sender has begun sending payload.
  mutual_acceptance_timeout_.reset();

  if (metadata.status() == TransferMetadata::Status::kComplete) {
    bool success = FinalizePayloads();
    return std::make_pair(/*completed=*/true, success);
  }

  // Update file paths during progress. It may impact transfer speed.
  // TODO: b/289290115 - Revisit UpdateFilePath to enhance transfer speed for
  // MacOS.
  if (update_file_paths_in_progress) {
    UpdateFilePayloadPaths();
  } else {
    if (metadata.status() == TransferMetadata::Status::kCancelled) {
      NL_VLOG(1) << __func__ << ": Update file paths for cancelled transfer";
      UpdateFilePayloadPaths();
    }
  }

  // Make sure to call this before calling Disconnect, or we risk losing some
  // transfer updates in the receive case due to the Disconnect call cleaning up
  // share targets.
  UpdateTransferMetadata(metadata);

  if (TransferMetadata::IsFinalStatus(metadata.status())) {
    // Cancellation has its own disconnection strategy, possibly adding a
    // delay before disconnection to provide the other party time to process
    // the cancellation.
    if (metadata.status() != TransferMetadata::Status::kCancelled) {
      Disconnect();
    }
  }
  return std::make_pair(/*completed=*/false, /*success=*/false);
}

}  // namespace nearby::sharing
