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
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/constants.h"
#include "sharing/incoming_frames_reader.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_sharing_decoder.h"
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

}  // namespace

ShareTargetInfo::ShareTargetInfo(
    std::string endpoint_id, const ShareTarget& share_target)
    : endpoint_id_(std::move(endpoint_id)),
      self_share_(share_target.for_self_share),
      share_target_(share_target) {}

ShareTargetInfo::ShareTargetInfo(ShareTargetInfo&&) = default;

ShareTargetInfo& ShareTargetInfo::operator=(ShareTargetInfo&&) = default;

ShareTargetInfo::~ShareTargetInfo() = default;

void ShareTargetInfo::UpdateTransferMetadata(
    const TransferMetadata& transfer_metadata) {
  if (got_final_status_) {
    // If we already got a final status, we can ignore any subsequent final
    // statuses caused by race conditions.
    NL_VLOG(1)
        << __func__ << ": Transfer update decorator swallowed "
        << "status update because a final status was already received: "
        << share_target_.id << ": "
        << TransferMetadata::StatusToString(transfer_metadata.status());
    return;
  }
  got_final_status_ = transfer_metadata.is_final_status();
  InvokeTransferUpdateCallback(transfer_metadata);
}

void ShareTargetInfo::set_disconnect_status(
    TransferMetadata::Status disconnect_status) {
  disconnect_status_ = disconnect_status;
  if (disconnect_status_ != TransferMetadata::Status::kUnknown &&
      !TransferMetadata::IsFinalStatus(disconnect_status_)) {
    NL_LOG(DFATAL) << "Disconnect status is not final: "
                   << static_cast<int>(disconnect_status_);
  }
}

bool ShareTargetInfo::OnConnected(absl::Time connect_start_time,
                                  NearbyConnection* connection) {
  if (!OnNewConnection(connection)) {
    return false;
  }
  connection_start_time_ = connect_start_time;
  connection_ = connection;
  return true;
}

void ShareTargetInfo::RunPairedKeyVerification(
    Context* context, NearbySharingDecoder* decoder, OSType os_type,
    const PairedKeyVerificationRunner::VisibilityHistory& visibility_history,
    NearbyShareCertificateManager* certificate_manager,
    std::optional<std::vector<uint8_t>> token,
    std::function<void(PairedKeyVerificationRunner::PairedKeyVerificationResult,
                       OSType)>
        callback) {
  if (!token) {
    NL_VLOG(1) << __func__
               << ": Failed to read authentication token from endpoint - "
               << endpoint_id_;
    std::move(callback)(
        PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail,
        OSType::UNKNOWN_OS_TYPE);
    return;
  }

  frames_reader_ =
      std::make_shared<IncomingFramesReader>(context, decoder, connection_);

  key_verification_runner_ = std::make_shared<PairedKeyVerificationRunner>(
      context->GetClock(), os_type, IsIncoming(), visibility_history, *token,
      connection_, certificate_, certificate_manager, frames_reader_.get(),
      kReadFramesTimeout);
  key_verification_runner_->Run(std::move(callback));
}

void ShareTargetInfo::OnDisconnect() {
  if (disconnect_status_ != TransferMetadata::Status::kUnknown) {
    UpdateTransferMetadata(
        TransferMetadataBuilder().set_status(disconnect_status_).build());
  }
  connection_ = nullptr;
}

void ShareTargetInfo::SetAttachmentPayloadId(int64_t attachment_id,
                                             int64_t payload_id) {
  attachment_payload_map_[attachment_id] = payload_id;
}

void ShareTargetInfo::CancelPayloads(
    NearbyConnectionsManager& connections_manager) {
  for (const auto& [attachment_id, payload_id] : attachment_payload_map_) {
    connections_manager.Cancel(payload_id);
  }
}

void ShareTargetInfo::WriteFrame(const Frame& frame) {
  if (connection_ == nullptr) {
    NL_LOG(WARNING) << __func__ << ": Failed to write response frame, due to "
                       "no connection established.";
    return;
  }
  std::vector<uint8_t> data(frame.ByteSizeLong());
  frame.SerializeToArray(data.data(), frame.ByteSizeLong());

  connection_->Write(std::move(data));
}

void ShareTargetInfo::WriteResponseFrame(
    ConnectionResponseFrame::Status response_status) {
  Frame frame;
  frame.set_version(Frame::V1);
  V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::RESPONSE);
  v1_frame->mutable_connection_response()->set_status(response_status);

  WriteFrame(frame);
}

void ShareTargetInfo::WriteCancelFrame() {
  NL_LOG(INFO) << __func__ << ": Writing cancel frame.";

  Frame frame;
  frame.set_version(Frame::V1);
  V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::CANCEL);

  WriteFrame(frame);
}

}  // namespace nearby::sharing
