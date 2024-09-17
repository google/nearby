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

#include "sharing/share_session.h"

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "internal/platform/clock.h"
#include "internal/platform/task_runner.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/constants.h"
#include "sharing/incoming_frames_reader.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"

namespace nearby::sharing {
namespace {

using ::location::nearby::proto::sharing::OSType;
using ::nearby::sharing::service::proto::ConnectionResponseFrame;
using ::nearby::sharing::service::proto::Frame;
using ::nearby::sharing::service::proto::V1Frame;

// Used to hash a token into a 4 digit string.
constexpr int kHashModulo = 9973;
constexpr int kHashBaseMultiplier = 31;

// Converts authentication token to four bytes digit string.
std::string TokenToFourDigitString(const std::vector<uint8_t>& bytes) {
  int hash = 0;
  int multiplier = 1;
  for (uint8_t byte : bytes) {
    // Java bytes are signed two's complement so cast to use the correct sign.
    hash = (hash + static_cast<int8_t>(byte) * multiplier) % kHashModulo;
    multiplier = (multiplier * kHashBaseMultiplier) % kHashModulo;
  }

  return absl::StrFormat("%04d", std::abs(hash));
}

}  // namespace

ShareSession::ShareSession(TaskRunner& service_thread,
                           analytics::AnalyticsRecorder& analytics_recorder,
                           std::string endpoint_id,
                           const ShareTarget& share_target)
    : service_thread_(service_thread),
      analytics_recorder_(analytics_recorder),
      endpoint_id_(std::move(endpoint_id)),
      self_share_(share_target.for_self_share),
      share_target_(share_target) {}

ShareSession::ShareSession(ShareSession&&) = default;

ShareSession::~ShareSession() = default;

void ShareSession::UpdateTransferMetadata(
    const TransferMetadata& transfer_metadata) {
  if (got_final_status_) {
    // If we already got a final status, we can ignore any subsequent final
    // statuses caused by race conditions.
    NL_VLOG(1) << __func__ << ": Transfer update decorator swallowed "
               << "status update because a final status was already received: "
               << share_target_.id << ": "
               << TransferMetadata::StatusToString(transfer_metadata.status());
    return;
  }
  got_final_status_ = transfer_metadata.is_final_status();
  InvokeTransferUpdateCallback(transfer_metadata);
}

std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>
ShareSession::payload_tracker() const {
  if (!payload_tracker_) {
    return std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>();
  }
  return payload_tracker_->GetWeakPtr();
}

void ShareSession::set_disconnect_status(
    TransferMetadata::Status disconnect_status) {
  disconnect_status_ = disconnect_status;
  if (disconnect_status_ != TransferMetadata::Status::kUnknown &&
      !TransferMetadata::IsFinalStatus(disconnect_status_)) {
    NL_LOG(DFATAL) << "Disconnect status is not final: "
                   << static_cast<int>(disconnect_status_);
  }
}

bool ShareSession::OnConnected(absl::Time connect_start_time,
                               NearbyConnectionsManager* connections_manager,
                               NearbyConnection* connection) {
  NL_DCHECK(connections_manager) << "Connections manager must not be null";
  connections_manager_ = connections_manager;
  if (!OnNewConnection(connection)) {
    return false;
  }
  connection_start_time_ = connect_start_time;
  connection_ = connection;
  frames_reader_ =
      std::make_shared<IncomingFramesReader>(service_thread_, connection_);
  return true;
}

void ShareSession::Disconnect() {
  if (connection_ == nullptr) {
    return;
  }
  // Do not clear connection_ here.  It will be cleared in OnDisconnect().
  connection_->Close();
}

void ShareSession::Abort(TransferMetadata::Status status) {
  NL_DCHECK(TransferMetadata::IsFinalStatus(status))
      << "Abort should only be called with a final status";

  // First invoke the appropriate transfer callback with the final
  // |status|.
  UpdateTransferMetadata(TransferMetadataBuilder().set_status(status).build());

  // Close connection if necessary.
  if (connection_ == nullptr) {
    return;
  }
  // Final status already sent above.  No need to send it again.
  set_disconnect_status(TransferMetadata::Status::kUnknown);
  connection_->Close();
}

void ShareSession::RunPairedKeyVerification(
    Clock* clock, OSType os_type,
    const PairedKeyVerificationRunner::VisibilityHistory& visibility_history,
    NearbyShareCertificateManager* certificate_manager,
    const std::vector<uint8_t>& token,
    std::function<void(PairedKeyVerificationRunner::PairedKeyVerificationResult,
                       OSType)>
        callback) {
  token_ = TokenToFourDigitString(token);

  key_verification_runner_ = std::make_shared<PairedKeyVerificationRunner>(
      clock, os_type, IsIncoming(), visibility_history, token,
      connection_, certificate_, certificate_manager, frames_reader_.get(),
      kReadFramesTimeout);
  key_verification_runner_->Run(std::move(callback));
}

void ShareSession::OnDisconnect() {
  OnConnectionDisconnected();
  if (disconnect_status_ != TransferMetadata::Status::kUnknown) {
    UpdateTransferMetadata(
        TransferMetadataBuilder().set_status(disconnect_status_).build());
  }
  connection_ = nullptr;
}

void ShareSession::SetAttachmentPayloadId(int64_t attachment_id,
                                          int64_t payload_id) {
  attachment_payload_map_[attachment_id] = payload_id;
}

void ShareSession::CancelPayloads() {
  if (connections_manager_ == nullptr) {
    return;
  }
  for (const auto& [attachment_id, payload_id] : attachment_payload_map_) {
    connections_manager_->Cancel(payload_id);
  }
}

void ShareSession::WriteFrame(const Frame& frame) {
  if (connection_ == nullptr) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to write response frame, due to "
                       "no connection established.";
    return;
  }
  std::vector<uint8_t> data(frame.ByteSizeLong());
  frame.SerializeToArray(data.data(), frame.ByteSizeLong());

  connection_->Write(std::move(data));
}

void ShareSession::WriteResponseFrame(
    ConnectionResponseFrame::Status response_status) {
  Frame frame;
  frame.set_version(Frame::V1);
  V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::RESPONSE);
  v1_frame->mutable_connection_response()->set_status(response_status);

  WriteFrame(frame);
}

void ShareSession::WriteCancelFrame() {
  NL_LOG(INFO) << __func__ << ": Writing cancel frame.";

  Frame frame;
  frame.set_version(Frame::V1);
  V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::CANCEL);

  WriteFrame(frame);
}

bool ShareSession::HandleKeyVerificationResult(
    PairedKeyVerificationRunner::PairedKeyVerificationResult result,
    location::nearby::proto::sharing::OSType share_target_os_type) {
  os_type_ = share_target_os_type;

  switch (result) {
    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail:
      NL_LOG(WARNING) << __func__ << ": Paired key handshake failed for target "
                      << share_target().id << ". Disconnecting.";
      return false;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess:
      NL_VLOG(1) << __func__ << ": Paired key handshake succeeded for target - "
                 << share_target().id;
      // If verification succeeds, this either means that the target is a
      // self-share or a mutual contact. In either case, we should clear the
      // token.
      token_.resize(0);
      break;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable:
      NL_VLOG(1) << __func__
                 << ": Unable to verify paired key encryption when "
                    "receiving connection from target - "
                 << share_target().id;
      // If we are unable to verify the paired key, we should clear the self
      // share flag.
      self_share_ = false;
      break;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnknown:
      NL_LOG(WARNING) << __func__
                      << ": Unknown PairedKeyVerificationResult for target "
                      << share_target().id << ". Disconnecting.";
      return false;
  }
  return true;
}

}  // namespace nearby::sharing
