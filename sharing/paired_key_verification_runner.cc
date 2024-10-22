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

#include "sharing/paired_key_verification_runner.h"

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "internal/platform/clock.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/certificates/common.h"
#include "sharing/certificates/constants.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/incoming_frames_reader.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/proto/timestamp.pb.h"
#include "sharing/proto/wire_format.pb.h"

namespace nearby {
namespace sharing {

using ::location::nearby::proto::sharing::OSType;
using ::nearby::sharing::proto::DeviceVisibility;
using ::nearby::sharing::service::proto::Frame;
using ::nearby::sharing::service::proto::PairedKeyEncryptionFrame;
using ::nearby::sharing::service::proto::PairedKeyResultFrame;
using ::nearby::sharing::service::proto::V1Frame;

namespace {

// The size of the random byte array used for the encryption frame's signed data
// if a valid signature cannot be generated. This size is consistent with the
// GmsCore implementation.
const size_t kNearbyShareNumBytesRandomSignature = 72;
constexpr absl::Duration kRelaxAfterSetVisibilityTimeout = absl::Minutes(15);

PairedKeyVerificationRunner::PairedKeyVerificationResult Convert(
    nearby::sharing::service::proto::PairedKeyResultFrame::Status status) {
  switch (status) {
    case PairedKeyResultFrame::UNKNOWN:
      return PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnknown;

    case PairedKeyResultFrame::SUCCESS:
      return PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess;

    case PairedKeyResultFrame::FAIL:
      return PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail;

    case PairedKeyResultFrame::UNABLE:
      return PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable;
  }
}

std::vector<uint8_t> PadPrefix(char prefix, std::vector<uint8_t> bytes) {
  bytes.insert(bytes.begin(), prefix);
  return bytes;
}

}  // namespace

std::ostream& operator<<(
    std::ostream& out,
    const PairedKeyVerificationRunner::PairedKeyVerificationResult& obj) {
  out << static_cast<std::underlying_type<
      PairedKeyVerificationRunner::PairedKeyVerificationResult>::type>(obj);
  return out;
}

PairedKeyVerificationRunner::PairedKeyVerificationRunner(
    Clock* clock, OSType os_type, bool share_target_is_incoming,
    const VisibilityHistory& visibility_history,
    const std::vector<uint8_t>& token, NearbyConnection* connection,
    const std::optional<NearbyShareDecryptedPublicCertificate>& certificate,
    NearbyShareCertificateManager* certificate_manager,
    IncomingFramesReader* frames_reader, absl::Duration read_frame_timeout)
    : clock_(clock),
      os_type_(os_type),
      raw_token_(token),
      connection_(connection),
      certificate_(certificate),
      certificate_manager_(certificate_manager),
      frames_reader_(frames_reader),
      read_frame_timeout_(read_frame_timeout) {
  NL_DCHECK(clock_);
  NL_DCHECK(connection);
  NL_DCHECK(certificate_manager);
  NL_DCHECK(frames_reader);

  if (share_target_is_incoming) {
    local_prefix_ = kNearbyShareReceiverVerificationPrefix;
    remote_prefix_ = kNearbyShareSenderVerificationPrefix;
    visibility_history_ = visibility_history;
  } else {
    remote_prefix_ = kNearbyShareReceiverVerificationPrefix;
    local_prefix_ = kNearbyShareSenderVerificationPrefix;
    // Sender always uses ALL_CONTACTS cert to sign and verify signature.
    visibility_history_ = {
        .visibility = DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
        .last_visibility = DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
        .last_visibility_time = absl::UnixEpoch(),
    };
  }
}

PairedKeyVerificationRunner::~PairedKeyVerificationRunner() = default;

void PairedKeyVerificationRunner::Run(
    std::function<void(PairedKeyVerificationResult, OSType)> callback) {
  NL_DCHECK(!callback_);
  callback_ = std::move(callback);
  verification_result_ = PairedKeyVerificationResult::kSuccess;

  SendPairedKeyEncryptionFrame();
  frames_reader_->ReadFrame(
      V1Frame::PAIRED_KEY_ENCRYPTION,
      [&, runner = GetWeakPtr()](std::optional<V1Frame> frame) {
        auto verification_runner = runner.lock();
        if (verification_runner == nullptr) {
          NL_LOG(WARNING) << "PairedKeyVerificationRunner is released before.";
          return;
        }
        OnReadPairedKeyEncryptionFrame(std::move(frame));
      },
      read_frame_timeout_);
}

void PairedKeyVerificationRunner::OnReadPairedKeyEncryptionFrame(
    std::optional<V1Frame> frame) {
  if (!frame.has_value()) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to read remote paired key encryption";
    std::move(callback_)(PairedKeyVerificationResult::kFail,
                         OSType::UNKNOWN_OS_TYPE);
    return;
  }

  PairedKeyVerificationResult auth_token_hash_result =
      VerifyAuthTokenHashWithPrivateCertificate(visibility_history_.visibility,
                                                *frame);

  if (auth_token_hash_result != PairedKeyVerificationResult::kSuccess) {
    if (IsVisibilityRecentlyUpdated()) {
      auth_token_hash_result = VerifyAuthTokenHashWithPrivateCertificate(
          visibility_history_.last_visibility, *frame);
    }
  }

  ApplyResult(auth_token_hash_result);
  NL_VLOG(1) << __func__ << ": Remote public certificate verification result "
             << auth_token_hash_result;

  PairedKeyVerificationResult local_result =
      VerifyPairedKeyEncryptionFrame(*frame);
  ApplyResult(local_result);
  NL_VLOG(1) << __func__ << ": Paired key encryption verification result "
             << local_result;

  SendPairedKeyResultFrame(local_result);

  frames_reader_->ReadFrame(
      V1Frame::PAIRED_KEY_RESULT,
      [this, runner = GetWeakPtr()](std::optional<V1Frame> frame) {
        auto verification_runner = runner.lock();
        if (verification_runner == nullptr) {
          NL_LOG(WARNING) << "PairedKeyVerificationRunner is released before.";
          return;
        }
        OnReadPairedKeyResultFrame(std::move(frame));
      },
      read_frame_timeout_);
}

void PairedKeyVerificationRunner::OnReadPairedKeyResultFrame(
    std::optional<V1Frame> frame) {
  if (!frame.has_value()) {
    NL_LOG(WARNING) << __func__ << ": Failed to read remote paired key result";
    std::move(callback_)(PairedKeyVerificationResult::kFail,
                         OSType::UNKNOWN_OS_TYPE);
    return;
  }

  PairedKeyVerificationResult remote_result =
      Convert(frame->paired_key_result().status());
  ApplyResult(remote_result);
  NL_VLOG(1) << __func__ << ": Paired key result frame result "
             << remote_result;

  NL_VLOG(1) << __func__ << ": Combined verification result "
             << verification_result_;

  OSType os_type = OSType::UNKNOWN_OS_TYPE;
  if (frame->paired_key_result().has_os_type()) {
    os_type = frame->paired_key_result().os_type();
  }

  std::move(callback_)(verification_result_, os_type);
}

void PairedKeyVerificationRunner::SendPairedKeyResultFrame(
    PairedKeyVerificationResult result) {
  Frame frame;
  frame.set_version(Frame::V1);
  V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::PAIRED_KEY_RESULT);
  PairedKeyResultFrame* result_frame = v1_frame->mutable_paired_key_result();

  switch (result) {
    case PairedKeyVerificationResult::kUnable:
      result_frame->set_status(PairedKeyResultFrame::UNABLE);
      break;

    case PairedKeyVerificationResult::kSuccess:
      result_frame->set_status(PairedKeyResultFrame::SUCCESS);
      break;

    case PairedKeyVerificationResult::kFail:
      result_frame->set_status(PairedKeyResultFrame::FAIL);
      break;

    case PairedKeyVerificationResult::kUnknown:
      result_frame->set_status(PairedKeyResultFrame::UNKNOWN);
      break;
  }

  // Set OS type to allow remote device knowns the paring device OS type.
  result_frame->set_os_type(os_type_);

  std::vector<uint8_t> data(frame.ByteSizeLong());
  frame.SerializeToArray(data.data(), frame.ByteSizeLong());

  connection_->Write(std::move(data));
}

void PairedKeyVerificationRunner::SendPairedKeyEncryptionFrame() {
  std::optional<std::vector<uint8_t>> signature =
      certificate_manager_->SignWithPrivateCertificate(
          visibility_history_.visibility, PadPrefix(local_prefix_, raw_token_));
  if (!signature.has_value() || signature->empty()) {
    signature = GenerateRandomBytes(kNearbyShareNumBytesRandomSignature);
  }

  std::vector<uint8_t> certificate_id_hash;
  if (certificate_.has_value()) {
    certificate_id_hash = certificate_->HashAuthenticationToken(raw_token_);
  }
  if (certificate_id_hash.empty()) {
    certificate_id_hash =
        GenerateRandomBytes(kNearbyShareNumBytesAuthenticationTokenHash);
  }

  Frame frame;
  frame.set_version(Frame::V1);
  V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::PAIRED_KEY_ENCRYPTION);
  PairedKeyEncryptionFrame* encryption_frame =
      v1_frame->mutable_paired_key_encryption();
  encryption_frame->set_signed_data(signature->data(), signature->size());
  if (IsVisibilityRecentlyUpdated()) {
    NL_LOG(INFO)
        << "Attempts to sign authentication token with a previous private key.";
    std::optional<std::vector<uint8_t>> optional_signature =
        certificate_manager_->SignWithPrivateCertificate(
            visibility_history_.last_visibility,
            PadPrefix(local_prefix_, raw_token_));

    if (optional_signature.has_value()) {
      encryption_frame->set_optional_signed_data(optional_signature->data(),
                                                 optional_signature->size());
    }
  }
  encryption_frame->set_secret_id_hash(certificate_id_hash.data(),
                                       certificate_id_hash.size());
  std::vector<uint8_t> data(frame.ByteSizeLong());
  frame.SerializeToArray(data.data(), frame.ByteSizeLong());

  connection_->Write(std::move(data));
}

PairedKeyVerificationRunner::PairedKeyVerificationResult
PairedKeyVerificationRunner::VerifyAuthTokenHashWithPrivateCertificate(
    DeviceVisibility visibility,
    const nearby::sharing::service::proto::V1Frame& frame) {
  std::optional<std::vector<uint8_t>> hash =
      certificate_manager_->HashAuthenticationTokenWithPrivateCertificate(
          visibility, raw_token_);

  const std::string& frame_hash =
      frame.paired_key_encryption().secret_id_hash();
  std::vector<uint8_t> frame_hash_data{frame_hash.begin(), frame_hash.end()};

  if (hash.has_value() && *hash == frame_hash_data) {
    NL_VLOG(1) << __func__
               << ": Successfully verified remote public certificate.";
    return PairedKeyVerificationResult::kSuccess;
  }

  NL_VLOG(1) << __func__ << ": Unable to verify remote public certificate.";
  return PairedKeyVerificationResult::kUnable;
}

PairedKeyVerificationRunner::PairedKeyVerificationResult
PairedKeyVerificationRunner::VerifyPairedKeyEncryptionFrame(
    const V1Frame& frame) {
  if (!certificate_) {
    NL_VLOG(1) << __func__
               << ": Unable to verify remote paired key encryption frame. "
                  "Remote side is not a known share target.";
    return PairedKeyVerificationResult::kUnable;
  }

  auto signed_data = frame.paired_key_encryption().signed_data();
  std::vector<uint8_t> data(signed_data.begin(), signed_data.end());
  if (!certificate_->VerifySignature(PadPrefix(remote_prefix_, raw_token_),
                                     data)) {
    if (!frame.paired_key_encryption().has_optional_signed_data()) {
      NL_LOG(WARNING)
          << __func__
          << ": Unable to verify remote paired key encryption frame. "
             "no optional signed data.";
      return PairedKeyVerificationResult::kFail;
    }
    // Verify optional signed data.
    auto optional_signed_data =
        frame.paired_key_encryption().optional_signed_data();
    std::vector<uint8_t> optional_data(optional_signed_data.begin(),
                                       optional_signed_data.end());
    if (certificate_->VerifySignature(PadPrefix(remote_prefix_, raw_token_),
                                      optional_data)) {
      NL_LOG(INFO) << "Successfully verified remote paired key encryption "
                      "frame with the optional signed data.";
    } else {
      NL_LOG(WARNING)
          << __func__
          << ": Unable to verify remote paired key encryption frame.";
      return PairedKeyVerificationResult::kFail;
    }
  }

  NL_VLOG(1) << __func__
             << ": Successfully verified remote paired key encryption frame.";
  return PairedKeyVerificationResult::kSuccess;
}

void PairedKeyVerificationRunner::ApplyResult(
    PairedKeyVerificationResult result) {
  if (verification_result_ == PairedKeyVerificationResult::kFail) {
    // If already failed, then nothing to do.
    return;
  }
  switch (result) {
    case PairedKeyVerificationResult::kSuccess:
      // Success does not change final result.
      break;
    case PairedKeyVerificationResult::kFail:
      // If new result is kFail, then the whole transaction failed.
      verification_result_ = PairedKeyVerificationResult::kFail;
      break;
    case PairedKeyVerificationResult::kUnable:
      verification_result_ = PairedKeyVerificationResult::kUnable;
      break;
    case PairedKeyVerificationResult::kUnknown:
    default:
      verification_result_ = PairedKeyVerificationResult::kUnable;
      break;
  }
}

bool PairedKeyVerificationRunner::IsVisibilityRecentlyUpdated() const {
  return visibility_history_.visibility !=
             visibility_history_.last_visibility &&
         (clock_->Now() - visibility_history_.last_visibility_time <
          kRelaxAfterSetVisibilityTimeout);
}

}  // namespace sharing
}  // namespace nearby
