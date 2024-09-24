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

#include "sharing/outgoing_share_session.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>  // NOLINT
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/platform/clock.h"
#include "internal/platform/task_runner.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment_container.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/constants.h"
#include "sharing/file_attachment.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/nearby_file_handler.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/payload_tracker.h"
#include "sharing/share_session.h"
#include "sharing/share_target.h"
#include "sharing/text_attachment.h"
#include "sharing/thread_timer.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby::sharing {

using ::nearby::sharing::service::proto::ConnectionResponseFrame;
using ::nearby::sharing::service::proto::Frame;
using ::nearby::sharing::service::proto::IntroductionFrame;
using ::nearby::sharing::service::proto::V1Frame;

OutgoingShareSession::OutgoingShareSession(
    TaskRunner& service_thread,
    analytics::AnalyticsRecorder& analytics_recorder, std::string endpoint_id,
    const ShareTarget& share_target,
    std::function<void(OutgoingShareSession&, const TransferMetadata&)>
        transfer_update_callback)
    : ShareSession(service_thread, analytics_recorder, std::move(endpoint_id),
                   share_target),
      transfer_update_callback_(std::move(transfer_update_callback)) {}

OutgoingShareSession::OutgoingShareSession(OutgoingShareSession&&) =
    default;

OutgoingShareSession::~OutgoingShareSession() = default;

void OutgoingShareSession::InvokeTransferUpdateCallback(
    const TransferMetadata& metadata) {
  transfer_update_callback_(*this, metadata);
}

bool OutgoingShareSession::ProcessKeyVerificationResult(
    PairedKeyVerificationRunner::PairedKeyVerificationResult result,
    location::nearby::proto::sharing::OSType share_target_os_type) {
  return HandleKeyVerificationResult(result, share_target_os_type);
}

bool OutgoingShareSession::OnNewConnection(NearbyConnection* connection) {
  if (!connection) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to initiate connection to share target "
                    << share_target().id;
    if (connection_layer_status_ == Status::kTimeout) {
      set_disconnect_status(TransferMetadata::Status::kTimedOut);
      connection_layer_status_ = Status::kUnknown;
    } else {
      set_disconnect_status(TransferMetadata::Status::kFailed);
    }
    return false;
  }
  set_disconnect_status(TransferMetadata::Status::kFailed);
  return true;
}

void OutgoingShareSession::OnConnectionDisconnected() {
  if (pending_complete_metadata_.has_value()) {
    UpdateTransferMetadata(*pending_complete_metadata_);
    pending_complete_metadata_.reset();
  }
}


std::vector<std::filesystem::path> OutgoingShareSession::GetFilePaths()
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

void OutgoingShareSession::CreateTextPayloads() {
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

void OutgoingShareSession::CreateWifiCredentialsPayloads() {
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

bool OutgoingShareSession::CreateFilePayloads(
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

bool OutgoingShareSession::FillIntroductionFrame(
    IntroductionFrame* introduction) const {
  const AttachmentContainer& container = attachment_container();
  if (!container.HasAttachments()) {
    return false;
  }
  if (file_payloads_.size() != container.GetFileAttachments().size() ||
      text_payloads_.size() != container.GetTextAttachments().size() ||
      wifi_credentials_payloads_.size() !=
          container.GetWifiCredentialsAttachments().size()) {
    return false;
  }
  // Write introduction of file payloads.
  const std::vector<FileAttachment>& file_attachments =
      container.GetFileAttachments();
  for (int i = 0; i < file_attachments.size(); ++i) {
    const FileAttachment& file = file_attachments[i];
    auto* file_metadata = introduction->add_file_metadata();
    file_metadata->set_id(file.id());
    file_metadata->set_name(std::string(file.file_name()));
    file_metadata->set_payload_id(file_payloads_[i].id);
    file_metadata->set_type(file.type());
    file_metadata->set_mime_type(std::string(file.mime_type()));
    file_metadata->set_size(file.size());
  }

  // Write introduction of text payloads.
  const std::vector<TextAttachment>& text_attachments =
      container.GetTextAttachments();
  for (int i = 0; i < text_attachments.size(); ++i) {
    const TextAttachment& text = text_attachments[i];
    auto* text_metadata = introduction->add_text_metadata();
    text_metadata->set_id(text.id());
    text_metadata->set_text_title(std::string(text.text_title()));
    text_metadata->set_type(text.type());
    text_metadata->set_size(text.size());
    text_metadata->set_payload_id(text_payloads_[i].id);
  }

  // Write introduction of Wi-Fi credentials payloads.
  const std::vector<WifiCredentialsAttachment>& wifi_credentials_attachments =
      container.GetWifiCredentialsAttachments();
  for (int i = 0; i < wifi_credentials_attachments.size(); ++i) {
    const WifiCredentialsAttachment& wifi_credentials =
        wifi_credentials_attachments[i];
    auto* wifi_credentials_metadata =
        introduction->add_wifi_credentials_metadata();
    wifi_credentials_metadata->set_id(wifi_credentials.id());
    wifi_credentials_metadata->set_ssid(std::string(wifi_credentials.ssid()));
    wifi_credentials_metadata->set_security_type(
        wifi_credentials.security_type());
    wifi_credentials_metadata->set_payload_id(wifi_credentials_payloads_[i].id);
  }
  return true;
}

bool OutgoingShareSession::AcceptTransfer(
    std::function<void(std::optional<ConnectionResponseFrame>)>
        response_callback) {
  if (!IsConnected()) {
    NL_LOG(WARNING) << __func__
                    << ": Accept invoked for unconnected share target";
    return false;
  }
  if (!ready_for_accept_) {
    NL_LOG(WARNING) << __func__ << ": out of order API call.";
    return false;
  }
  ready_for_accept_ = false;
  // Wait for remote accept in response frame.
  UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_token(token())
          .set_status(TransferMetadata::Status::kAwaitingRemoteAcceptance)
          .build());
  NL_VLOG(1) << __func__ << ": Waiting for response frame from "
             << share_target().id;
  frames_reader()->ReadFrame(
      nearby::sharing::service::proto::V1Frame::RESPONSE,
      [callback = std::move(response_callback)](std::optional<V1Frame> frame) {
        if (!frame.has_value()) {
          callback(std::nullopt);
          return;
        }
        callback(frame->connection_response());
      },
      kReadResponseFrameTimeout);
  return true;
}

void OutgoingShareSession::SendPayloads(
    bool enable_transfer_cancellation_optimization, Clock* clock,
    std::function<
        void(std::optional<nearby::sharing::service::proto::V1Frame> frame)>
        frame_read_callback,
    std::function<void(int64_t, TransferMetadata)> update_callback) {
  if (!IsConnected()) {
    NL_LOG(WARNING) << __func__
                    << ": SendPayloads invoked for unconnected share target";
    return;
  }
  frames_reader()->ReadFrame(std::move(frame_read_callback));

  // Log analytics event of sending attachment start.
  analytics_recorder().NewSendAttachmentsStart(session_id(),
                                               attachment_container(),
                                               /*transfer_position=*/1,
                                               /*concurrent_connections=*/1);
  NL_VLOG(1) << __func__
             << ": The connection was accepted. Payloads are now being sent.";
  if (enable_transfer_cancellation_optimization) {
    InitSendPayload(clock, std::move(update_callback));
    SendNextPayload();
  } else {
    SendAllPayloads(clock, std::move(update_callback));
  }
}

void OutgoingShareSession::SendAllPayloads(
    Clock* clock,
    std::function<void(int64_t, TransferMetadata)> update_callback) {
  set_payload_tracker(std::make_unique<PayloadTracker>(
      clock, share_target().id, attachment_container(),
      attachment_payload_map(), std::move(update_callback)));
  for (auto& payload : ExtractTextPayloads()) {
    connections_manager()->Send(
        endpoint_id(), std::make_unique<Payload>(payload), payload_tracker());
  }
  for (auto& payload : ExtractFilePayloads()) {
    connections_manager()->Send(
        endpoint_id(), std::make_unique<Payload>(payload), payload_tracker());
  }
  for (auto& payload : ExtractWifiCredentialsPayloads()) {
    connections_manager()->Send(
        endpoint_id(), std::make_unique<Payload>(payload), payload_tracker());
  }
}

void OutgoingShareSession::InitSendPayload(
    Clock* clock,
    std::function<void(int64_t, TransferMetadata)> update_callback) {
  set_payload_tracker(std::make_unique<PayloadTracker>(
      clock, share_target().id, attachment_container(),
      attachment_payload_map(), std::move(update_callback)));
}

void OutgoingShareSession::SendNextPayload() {
  std::optional<Payload> payload = ExtractNextPayload();
  if (payload.has_value()) {
    NL_LOG(INFO) << __func__ << ": Send  payload " << payload->id;
    connections_manager()->Send(
        endpoint_id(), std::make_unique<Payload>(*payload), payload_tracker());
  } else {
    NL_LOG(WARNING) << __func__ << ": There is no paylaods to send.";
  }
}

bool OutgoingShareSession::SendIntroduction(
    std::function<void()> timeout_callback) {
  Frame frame;
  frame.set_version(Frame::V1);
  V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::INTRODUCTION);
  IntroductionFrame* introduction_frame = v1_frame->mutable_introduction();
  introduction_frame->set_start_transfer(true);
  if (!FillIntroductionFrame(introduction_frame)) {
    return false;
  }
  WriteFrame(frame);
  // Log analytics event of sending introduction.
  analytics_recorder().NewSendIntroduction(session_id(), share_target(),
                                           /*transfer_position=*/1,
                                           /*concurrent_connections=*/1,
                                           os_type());
  NL_VLOG(1) << __func__ << ": Successfully wrote the introduction frame";
  ready_for_accept_ = true;
  mutual_acceptance_timeout_ = std::make_unique<ThreadTimer>(
      service_thread(), "outgoing_mutual_acceptance_timeout",
      kReadResponseFrameTimeout, std::move(timeout_callback));
  return true;
}

std::optional<TransferMetadata::Status>
OutgoingShareSession::HandleConnectionResponse(
    std::optional<ConnectionResponseFrame> response) {
  // Stop accept timer.
  mutual_acceptance_timeout_.reset();

  if (!response.has_value()) {
    NL_LOG(WARNING)
        << __func__
        << ": Failed to read a response from the remote device. Disconnecting.";
    return TransferMetadata::Status::kFailed;
  }

  NL_VLOG(1) << __func__
             << ": Successfully read the connection response frame.";

  switch (response->status()) {
    case ConnectionResponseFrame::ACCEPT: {
      UpdateTransferMetadata(
          TransferMetadataBuilder()
              .set_status(TransferMetadata::Status::kInProgress)
              .build());
      return std::nullopt;
    }
    case ConnectionResponseFrame::REJECT:
      NL_VLOG(1)
          << __func__
          << ": The connection was rejected. The connection has been closed.";
      return TransferMetadata::Status::kRejected;
    case ConnectionResponseFrame::NOT_ENOUGH_SPACE:
      NL_VLOG(1) << __func__
                 << ": The connection was rejected because the remote device "
                    "does not have enough space for our attachments. The "
                    "connection has been closed.";
      return TransferMetadata::Status::kNotEnoughSpace;
    case ConnectionResponseFrame::UNSUPPORTED_ATTACHMENT_TYPE:
      NL_VLOG(1) << __func__
                 << ": The connection was rejected because the remote device "
                    "does not support the attachments we were sending. The "
                    "connection has been closed.";
      return TransferMetadata::Status::kUnsupportedAttachmentType;
    case ConnectionResponseFrame::TIMED_OUT:
      NL_VLOG(1) << __func__
                 << ": The connection was rejected because the remote device "
                    "timed out. The connection has been closed.";
      return TransferMetadata::Status::kTimedOut;
    default:
      NL_VLOG(1) << __func__
                 << ": The connection failed. The connection has been closed.";
      break;
  }
  return TransferMetadata::Status::kFailed;
}

std::vector<Payload> OutgoingShareSession::ExtractTextPayloads() {
  return std::move(text_payloads_);
}

std::vector<Payload> OutgoingShareSession::ExtractFilePayloads() {
  return std::move(file_payloads_);
}

std::vector<Payload> OutgoingShareSession::ExtractWifiCredentialsPayloads() {
  return std::move(wifi_credentials_payloads_);
}

std::optional<Payload> OutgoingShareSession::ExtractNextPayload() {
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

void OutgoingShareSession::DelayCompleteMetadata(
    const TransferMetadata& complete_metadata) {
  LOG(INFO)
      << "Delay complete notification until receiver disconnects for target "
      << share_target().id;
  pending_complete_metadata_ = complete_metadata;
  // Change kComplete status to kInProgress and update clients.
  TransferMetadataBuilder builder =
      TransferMetadataBuilder::Clone(complete_metadata);
  builder.set_status(TransferMetadata::Status::kInProgress);
  UpdateTransferMetadata(builder.build());
}

void OutgoingShareSession::DisconnectionTimeout() {
  VLOG(1) << "Disconnection delay timeed out for target " << share_target().id;
  pending_complete_metadata_.reset();
  Disconnect();
}

void OutgoingShareSession::UpdateSessionForDedup(
    const ShareTarget& share_target,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate,
    absl::string_view endpoint_id) {
  if (IsConnected()) return;
  set_share_target(share_target);
  set_endpoint_id(endpoint_id);
  if (certificate.has_value()) {
    set_certificate(std::move(certificate.value()));
  } else {
    clear_certificate();
  }
}
}  // namespace nearby::sharing
