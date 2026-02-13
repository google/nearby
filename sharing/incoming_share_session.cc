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
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/time/time.h"
#include "internal/base/file_path.h"
#include "internal/platform/clock.h"
#include "internal/platform/task_runner.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment_container.h"
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
using ::location::nearby::proto::sharing::ResponseToIntroduction;
using ::nearby::sharing::service::proto::AppMetadata;
using ::nearby::sharing::service::proto::ConnectionResponseFrame;
using ::nearby::sharing::service::proto::IntroductionFrame;
using ::nearby::sharing::service::proto::V1Frame;
using ::nearby::sharing::service::proto::WifiCredentials;

}  // namespace

IncomingShareSession::IncomingShareSession(
    Clock* clock, TaskRunner& service_thread,
    NearbyConnectionsManager* connections_manager,
    analytics::AnalyticsRecorder& analytics_recorder, std::string endpoint_id,
    const ShareTarget& share_target,
    std::function<void(const IncomingShareSession&, const TransferMetadata&)>
        transfer_update_callback)
    : ShareSession(clock, service_thread, connections_manager,
                   analytics_recorder, std::move(endpoint_id), share_target),
      transfer_update_callback_(std::move(transfer_update_callback)) {}

IncomingShareSession::IncomingShareSession(IncomingShareSession&&) = default;

IncomingShareSession::~IncomingShareSession() = default;

void IncomingShareSession::InvokeTransferUpdateCallback(
    const TransferMetadata& metadata) {
  transfer_update_callback_(*this, metadata);
}

std::optional<TransferMetadata::Status>
IncomingShareSession::ProcessIntroduction(
    const IntroductionFrame& introduction_frame) {
  int64_t file_size_sum = 0;
  int app_file_count = 0;
  for (const AppMetadata& apk : introduction_frame.app_metadata()) {
    app_file_count += apk.file_name_size();
  }
  AttachmentContainer::Builder builder;
  builder.ReserveAttachmentsCount(
      introduction_frame.text_metadata_size() + app_file_count,
      introduction_frame.file_metadata_size(),
      introduction_frame.wifi_credentials_metadata_size());
  for (const auto& file : introduction_frame.file_metadata()) {
    if (file.size() <= 0) {
      LOG(WARNING) << "Ignore introduction, due to invalid attachment size";
      return TransferMetadata::Status::kUnsupportedAttachmentType;
    }

    VLOG(1) << "Found file attachment: id=" << file.id()
            << ", type= " << file.type() << ", size=" << file.size()
            << ", payload_id=" << file.payload_id()
            << ", parent_folder=" << file.parent_folder()
            << ", mime_type=" << file.mime_type();
    builder.AddFileAttachment(
        FileAttachment(file.id(), file.size(), file.name(), file.mime_type(),
                       file.type(), file.parent_folder()));
    SetAttachmentPayloadId(file.id(), file.payload_id());

    if (std::numeric_limits<int64_t>::max() - file.size() < file_size_sum) {
      LOG(WARNING) << "Ignoring introduction, total file size overflowed 64 "
                      "bit integer.";
      return TransferMetadata::Status::kNotEnoughSpace;
    }
    file_size_sum += file.size();
  }

  for (const AppMetadata& apk : introduction_frame.app_metadata()) {
    if (apk.size() <= 0) {
      LOG(WARNING) << __func__
                   << ": Ignore introduction, due to invalid attachment size";
      return TransferMetadata::Status::kUnsupportedAttachmentType;
    }

    VLOG(1) << __func__ << ": Found app attachment: id=" << apk.id()
            << ", app_name=" << apk.app_name()
            << ", package_name=" << apk.package_name()
            << ", size=" << apk.size();
    if (std::numeric_limits<int64_t>::max() - apk.size() < file_size_sum) {
      LOG(WARNING) << __func__
                   << ": Ignoring introduction, total file size overflowed "
                      "64 bit integer.";
      return TransferMetadata::Status::kNotEnoughSpace;
    }
    // Map each apk file to a file attachment.
    for (int index = 0; index < apk.file_name_size(); ++index) {
      // Locally generate an attachment id for each apk file, and map it to the
      // payload id.
      FileAttachment apk_file(
          /*id=*/0, apk.file_size(index), apk.file_name(index),
          /*mime_type=*/"", service::proto::FileMetadata::ANDROID_APP);
      int64_t apk_file_id = apk_file.id();
      VLOG(1) << __func__ << ": Found app file name: " << apk.file_name(index)
              << ", attachment id=" << apk_file_id
              << ", file size=" << apk.file_size(index)
              << ", payload_id=" << apk.payload_id(index);
      builder.AddFileAttachment(std::move(apk_file));
      SetAttachmentPayloadId(apk_file_id, apk.payload_id(index));
    }
    file_size_sum += apk.size();
  }

  for (const auto& text : introduction_frame.text_metadata()) {
    if (text.size() <= 0) {
      LOG(WARNING) << "Ignore introduction, due to invalid attachment size";
      return TransferMetadata::Status::kUnsupportedAttachmentType;
    }

    VLOG(1) << "Found text attachment: id=" << text.id()
            << ", type= " << text.type() << ", size=" << text.size()
            << ", payload_id=" << text.payload_id();
    builder.AddTextAttachment(
        TextAttachment(text.id(), text.type(), text.text_title(), text.size()));
    SetAttachmentPayloadId(text.id(), text.payload_id());
  }

  if (kSupportReceivingWifiCredentials) {
    for (const auto& wifi_credentials :
         introduction_frame.wifi_credentials_metadata()) {
      VLOG(1) << "Found WiFi credentials attachment: id="
              << wifi_credentials.id() << ", ssid= " << wifi_credentials.ssid()
              << ", payload_id=" << wifi_credentials.payload_id();
      builder.AddWifiCredentialsAttachment(WifiCredentialsAttachment(
          wifi_credentials.id(), wifi_credentials.ssid(),
          wifi_credentials.security_type()));
      SetAttachmentPayloadId(wifi_credentials.id(),
                             wifi_credentials.payload_id());
    }
  }

  if (builder.Empty()) {
    LOG(WARNING) << __func__
                 << ": No attachment is found for this share target. It can "
                    "be result of unrecognizable attachment type";
    return TransferMetadata::Status::kUnsupportedAttachmentType;
  }
  mutable_attachment_container() = std::move(*builder.Build());
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
  LOG(INFO) << ":Waiting for introduction from " << share_target().id;

  frames_reader()->ReadFrame(
      V1Frame::INTRODUCTION,
      [callback = std::move(introduction_callback)](
          bool is_timeout, std::optional<V1Frame> frame) {
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
    std::function<void(bool is_timeout, std::optional<V1Frame> frame)>
        frame_read_callback) {
  if (!IsConnected()) {
    LOG(WARNING) << "ReadyForTransfer called when not connected";
    return false;
  }
  ready_for_accept_ = true;
  set_disconnect_status(TransferMetadata::Status::kFailed);

  mutual_acceptance_timeout_ = std::make_unique<ThreadTimer>(
      service_thread(), "incoming_mutual_acceptance_timeout",
      kReadResponseFrameTimeout, std::move(accept_timeout_callback));
  frames_reader()->ReadFrame(std::move(frame_read_callback),
                             absl::ZeroDuration());

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
    absl::AnyInvocable<void()> payload_transfer_updates_callback) {
  if (!ready_for_accept_ || !IsConnected()) {
    LOG(WARNING) << "AcceptTransfer call not expected";
    return false;
  }
  ready_for_accept_ = false;
  InitializePayloadTracker(std::move(payload_transfer_updates_callback));
  const absl::flat_hash_map<int64_t, int64_t>& payload_map =
      attachment_payload_map();
  // Register status listener for all payloads.
  for (auto it = payload_map.begin(); it != payload_map.end(); ++it) {
    VLOG(1) << "Started listening for progress on payload: " << it->second
            << " for attachment: " << it->first;

    connections_manager().RegisterPayloadStatusListener(it->second,
                                                        payload_tracker());

    VLOG(1) << __func__ << ": Accepted incoming files from share target - "
            << share_target().id;
  }
  WriteResponseFrame(ConnectionResponseFrame::ACCEPT);
  VLOG(1) << __func__ << ": Successfully wrote response frame";
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
    LOG(INFO) << "Upgrade bandwidth when sending accept.";
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
      LOG(WARNING) << "Payload id missing for file attachment: " << file.id();
      result = false;
      continue;
    }

    const Payload* incoming_payload =
        connections_manager().GetIncomingPayload(it->second);
    if (!incoming_payload || !incoming_payload->content.is_file()) {
      LOG(WARNING) << "No payload found for file attachment: " << file.id();
      result = false;
      continue;
    }

    FilePath file_path = incoming_payload->content.file_payload.file_path;
    VLOG(1) << __func__ << ": Updated file_path=" << file_path.ToString();
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
      LOG(WARNING) << "Payload id missing for text attachment: " << text.id();
      return false;
    }
    const Payload* incoming_payload =
        connections_manager().GetIncomingPayload(it->second);
    if (!incoming_payload || !incoming_payload->content.is_bytes()) {
      LOG(WARNING) << "No payload found for text attachment: " << text.id();
      return false;
    }

    std::vector<uint8_t> bytes = incoming_payload->content.bytes_payload.bytes;
    if (bytes.empty()) {
      LOG(WARNING) << "Incoming bytes is empty for text attachment: "
                   << text.id() << " with payload_id: " << it->second;
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
      LOG(WARNING) << "Payload id missing for WiFi credentials: "
                   << wifi_credentials_attachment.id();
      return false;
    }

    const Payload* incoming_payload =
        connections_manager().GetIncomingPayload(it->second);
    if (!incoming_payload || !incoming_payload->content.is_bytes()) {
      LOG(WARNING) << "No payload found for WiFi credentials: "
                   << wifi_credentials_attachment.id();
      return false;
    }

    std::vector<uint8_t> bytes = incoming_payload->content.bytes_payload.bytes;
    if (bytes.empty()) {
      LOG(WARNING) << "Incoming bytes is empty for WiFi credentials: "
                   << wifi_credentials_attachment.id()
                   << " with payload_id: " << it->second;
      return false;
    }

    WifiCredentials wifi_credentials;
    if (!wifi_credentials.ParseFromArray(bytes.data(), bytes.size())) {
      LOG(WARNING) << "Incoming bytes is invalid for WiFi credentials: "
                   << wifi_credentials_attachment.id()
                   << " with payload_id: " << it->second;
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

std::vector<FilePath> IncomingShareSession::GetPayloadFilePaths()
    const {
  std::vector<FilePath> file_paths;
  const AttachmentContainer& container = attachment_container();
  const absl::flat_hash_map<int64_t, int64_t>& attachment_paylod_map =
      attachment_payload_map();
  for (const auto& file : container.GetFileAttachments()) {
    if (!file.file_path().has_value()) continue;
    FilePath file_path = *file.file_path();
    VLOG(1) << __func__ << ": file_path=" << file_path.ToString();
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
    connections_manager().UpgradeBandwidth(endpoint_id());
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
  DCHECK(TransferMetadata::IsFinalStatus(status))
      << "SendFailureResponse should only be called with a final status";
  UpdateTransferMetadata(TransferMetadataBuilder().set_status(status).build());
}

std::optional<TransferMetadata>
IncomingShareSession::ProcessPayloadTransferUpdates(
    bool update_file_paths_in_progress) {
  std::queue<std::unique_ptr<PayloadTransferUpdate>> updates =
      payload_updates_queue()->ReadAll();
  VLOG(1) << "Processing " << updates.size() << " PayloadTransferUpdates";
  if (updates.empty()) {
    return std::nullopt;
  }
  // Cancel acceptance timer when payload transfer update is received.
  // This mean sender has begun sending payload.
  mutual_acceptance_timeout_ = nullptr;
  std::optional<TransferMetadata> metadata;
  // If there is a batch of updates in the queue, only return the latest
  // TransferMetadata.
  for (; !updates.empty(); updates.pop()) {
    metadata =
        get_payload_tracker()->ProcessPayloadUpdate(std::move(updates.front()));
    if (!metadata.has_value()) {
      continue;
    }

    if (metadata->status() == TransferMetadata::Status::kComplete) {
      if (!FinalizePayloads()) {
        return TransferMetadataBuilder()
            .set_status(TransferMetadata::Status::kIncompletePayloads)
            .build();
      }
      return metadata;
    }

    // Update file paths during progress. It may impact transfer speed.
    // TODO: b/289290115 - Revisit UpdateFilePath to enhance transfer speed for
    // MacOS.
    if (update_file_paths_in_progress) {
      UpdateFilePayloadPaths();
    } else {
      if (metadata->status() == TransferMetadata::Status::kCancelled) {
        VLOG(1) << __func__ << ": Update file paths for cancelled transfer";
        UpdateFilePayloadPaths();
      }
    }
  }
  return metadata;
}

void IncomingShareSession::OnConnected(NearbyConnection* connection) {
  set_disconnect_status(TransferMetadata::Status::kFailed);
  SetConnection(connection);
}

void IncomingShareSession::PushPayloadTransferUpdateForTest(
    std::unique_ptr<PayloadTransferUpdate> update) {
  payload_updates_queue()->Queue(std::move(update));
}

}  // namespace nearby::sharing
