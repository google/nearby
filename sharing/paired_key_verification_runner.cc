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

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/clock.h"
#include "internal/platform/device_info.h"
#include "internal/platform/implementation/device_info.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/certificates/common.h"
#include "sharing/certificates/constants.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/incoming_frames_reader.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/proto/timestamp.pb.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"

namespace nearby {
namespace sharing {

using ::location::nearby::proto::sharing::OSType;
using ::nearby::sharing::proto::DeviceVisibility;
using ::nearby::sharing::service::proto::CertificateInfoFrame;
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

OSType ToProtoOsType(::nearby::api::DeviceInfo::OsType os_type) {
  switch (os_type) {
    case ::nearby::api::DeviceInfo::OsType::kAndroid:
      return OSType::ANDROID;
    case ::nearby::api::DeviceInfo::OsType::kChromeOs:
      return OSType::CHROME_OS;
    case ::nearby::api::DeviceInfo::OsType::kWindows:
      return OSType::WINDOWS;
    case ::nearby::api::DeviceInfo::OsType::kIos:
      return OSType::IOS;
    case ::nearby::api::DeviceInfo::OsType::kMacOS:
      return OSType::MACOS;
    case ::nearby::api::DeviceInfo::OsType::kUnknown:
      break;
  }

  return OSType::UNKNOWN_OS_TYPE;
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
    Clock* clock,
    DeviceInfo& device_info,
    NearbyShareSettings* nearby_share_settings,
    bool self_share_feature_enabled, const ShareTarget& share_target,
    absl::string_view endpoint_id, const std::vector<uint8_t>& token,
    NearbyConnection* connection,
    const std::optional<NearbyShareDecryptedPublicCertificate>& certificate,
    NearbyShareCertificateManager* certificate_manager,
    bool restrict_to_contacts, IncomingFramesReader* frames_reader,
    absl::Duration read_frame_timeout)
    : clock_(clock),
      device_info_(device_info),
      nearby_share_settings_(nearby_share_settings),
      self_share_feature_enabled_(self_share_feature_enabled),
      share_target_(share_target),
      endpoint_id_(std::string(endpoint_id)),
      raw_token_(token),
      connection_(connection),
      certificate_(certificate),
      certificate_manager_(certificate_manager),
      restrict_to_contacts_(restrict_to_contacts),
      frames_reader_(frames_reader),
      read_frame_timeout_(read_frame_timeout) {
  NL_DCHECK(clock_);
  NL_DCHECK(nearby_share_settings);
  NL_DCHECK(connection);
  NL_DCHECK(certificate_manager);
  NL_DCHECK(frames_reader);

  if (share_target.is_incoming) {
    local_prefix_ = kNearbyShareReceiverVerificationPrefix;
    remote_prefix_ = kNearbyShareSenderVerificationPrefix;
    relax_restrict_to_contacts_ =
        RelaxRestrictToContactsIfNeeded() ||
        nearby_share_settings_->GetVisibility() ==
            DeviceVisibility::DEVICE_VISIBILITY_EVERYONE;
  } else {
    remote_prefix_ = kNearbyShareReceiverVerificationPrefix;
    local_prefix_ = kNearbyShareSenderVerificationPrefix;
  }
}

PairedKeyVerificationRunner::~PairedKeyVerificationRunner() = default;

void PairedKeyVerificationRunner::Run(
    std::function<void(PairedKeyVerificationResult, OSType)> callback) {
  NL_DCHECK(!callback_);
  callback_ = std::move(callback);

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

  std::vector<PairedKeyVerificationResult> verification_results;

  PairedKeyVerificationResult remote_public_certificate_result =
      VerifyRemotePublicCertificate(*frame);

  if (remote_public_certificate_result ==
      PairedKeyVerificationResult::kSuccess) {
    SendCertificateInfo();
  } else if (restrict_to_contacts_ && !relax_restrict_to_contacts_) {
    NL_VLOG(1) << __func__
                    << ": we are only allowing connections with contacts. "
                       "Rejecting connection from unknown ShareTarget - "
                    << share_target_.id;
    std::move(callback_)(PairedKeyVerificationResult::kFail,
                         OSType::UNKNOWN_OS_TYPE);
    return;
  } else if (relax_restrict_to_contacts_) {
    remote_public_certificate_result =
        VerifyRemotePublicCertificateRelaxed(*frame);
  }

  verification_results.push_back(remote_public_certificate_result);
  NL_VLOG(1) << __func__
                  << ": Remote public certificate verification result "
                  << remote_public_certificate_result;

  PairedKeyVerificationResult local_result =
      VerifyPairedKeyEncryptionFrame(*frame);
  verification_results.push_back(local_result);
  NL_VLOG(1) << __func__ << ": Paired key encryption verification result "
                  << local_result;

  SendPairedKeyResultFrame(local_result);

  frames_reader_->ReadFrame(
      V1Frame::PAIRED_KEY_RESULT,
      [&, runner = GetWeakPtr(),
       verification_results =
           std::move(verification_results)](std::optional<V1Frame> frame) {
        auto verification_runner = runner.lock();
        if (verification_runner == nullptr) {
          NL_LOG(WARNING) << "PairedKeyVerificationRunner is released before.";
          return;
        }
        OnReadPairedKeyResultFrame(verification_results, std::move(frame));
      },
      read_frame_timeout_);
}

void PairedKeyVerificationRunner::OnReadPairedKeyResultFrame(
    std::vector<PairedKeyVerificationResult> verification_results,
    std::optional<V1Frame> frame) {
  if (!frame.has_value()) {
    NL_LOG(WARNING) << __func__ << ": Failed to read remote paired key result";
    std::move(callback_)(PairedKeyVerificationResult::kFail,
                         OSType::UNKNOWN_OS_TYPE);
    return;
  }

  PairedKeyVerificationResult key_result =
      Convert(frame->paired_key_result().status());
  verification_results.push_back(key_result);
  NL_VLOG(1) << __func__ << ": Paired key result frame result "
                  << key_result;

  PairedKeyVerificationResult combined_result =
      MergeResults(verification_results);
  NL_VLOG(1) << __func__ << ": Combined verification result "
                  << combined_result;

  OSType os_type = OSType::UNKNOWN_OS_TYPE;
  if (frame->paired_key_result().has_os_type()) {
    os_type = frame->paired_key_result().os_type();
  }

  std::move(callback_)(combined_result, os_type);
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
  result_frame->set_os_type(ToProtoOsType(device_info_.GetOsType()));

  std::vector<uint8_t> data(frame.ByteSize());
  frame.SerializeToArray(data.data(), frame.ByteSize());

  connection_->Write(std::move(data));
}

void PairedKeyVerificationRunner::SendCertificateInfo() {
  if (self_share_feature_enabled_) return;

  std::vector<nearby::sharing::proto::PublicCertificate> certificates;

  if (certificates.empty()) return;

  Frame frame;
  frame.set_version(Frame::V1);
  V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::CERTIFICATE_INFO);
  CertificateInfoFrame* cert_frame = v1_frame->mutable_certificate_info();
  for (const auto& certificate : certificates) {
    nearby::sharing::service::proto::PublicCertificate* cert =
        cert_frame->add_public_certificate();
    cert->set_secret_id(certificate.secret_id());
    cert->set_authenticity_key(certificate.secret_key());
    cert->set_public_key(certificate.public_key());
    cert->set_start_time(certificate.start_time().seconds() * 1000);
    cert->set_end_time(certificate.end_time().seconds() * 1000);
    cert->set_encrypted_metadata_bytes(certificate.encrypted_metadata_bytes());
    cert->set_metadata_encryption_key_tag(
        certificate.metadata_encryption_key_tag());
  }

  std::vector<uint8_t> data(frame.ByteSize());
  frame.SerializeToArray(data.data(), frame.ByteSize());

  connection_->Write(std::move(data));
}

void PairedKeyVerificationRunner::SendPairedKeyEncryptionFrame() {
  std::optional<std::vector<uint8_t>> signature =
      certificate_manager_->SignWithPrivateCertificate(
          nearby_share_settings_->GetVisibility(),
          PadPrefix(local_prefix_, raw_token_));
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
  if (RelaxRestrictToContactsIfNeeded()) {
    NL_LOG(INFO)
        << "Attempts to sign authentication token with a previous private key.";
    std::optional<std::vector<uint8_t>> optional_signature =
        certificate_manager_->SignWithPrivateCertificate(
            nearby_share_settings_->GetLastVisibility(),
            PadPrefix(local_prefix_, raw_token_));

    if (optional_signature.has_value()) {
      encryption_frame->set_optional_signed_data(optional_signature->data(),
                                                 optional_signature->size());
    }
  }
  encryption_frame->set_secret_id_hash(certificate_id_hash.data(),
                                       certificate_id_hash.size());
  std::vector<uint8_t> data(frame.ByteSize());
  frame.SerializeToArray(data.data(), frame.ByteSize());

  connection_->Write(std::move(data));
}

PairedKeyVerificationRunner::PairedKeyVerificationResult
PairedKeyVerificationRunner::VerifyRemotePublicCertificate(
    const V1Frame& frame) {
  return VerifyRemotePublicCertificateWithPrivateCertificate(
      nearby_share_settings_->GetVisibility(), frame);
}

PairedKeyVerificationRunner::PairedKeyVerificationResult
PairedKeyVerificationRunner::VerifyRemotePublicCertificateRelaxed(
    const nearby::sharing::service::proto::V1Frame& frame) {
  return VerifyRemotePublicCertificateWithPrivateCertificate(
      nearby_share_settings_->GetLastVisibility(), frame);
}

PairedKeyVerificationRunner::PairedKeyVerificationResult
PairedKeyVerificationRunner::
    VerifyRemotePublicCertificateWithPrivateCertificate(
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

  NL_VLOG(1) << __func__
                  << ": Unable to verify remote public certificate.";
  return PairedKeyVerificationResult::kUnable;
}

PairedKeyVerificationRunner::PairedKeyVerificationResult
PairedKeyVerificationRunner::VerifyPairedKeyEncryptionFrame(
    const V1Frame& frame) {
  if (!certificate_) {
    NL_VLOG(1) << __func__
                    << ": Unable to verify remote paired key encryption frame. "
                       "Certificate not found.";
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

    if (!RelaxRestrictToContactsIfNeeded()) {
      NL_LOG(WARNING)
          << __func__
          << ": Unable to verify remote paired key encryption frame. "
             "no need to try relax check.";
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

  if (!share_target_.is_known) {
    NL_LOG(INFO) << __func__
                 << ": Unable to verify remote paired key encryption frame. "
                    "Remote side is not a known share target.";
    return PairedKeyVerificationResult::kUnable;
  }

  NL_VLOG(1)
      << __func__
      << ": Successfully verified remote paired key encryption frame.";
  return PairedKeyVerificationResult::kSuccess;
}

PairedKeyVerificationRunner::PairedKeyVerificationResult
PairedKeyVerificationRunner::MergeResults(
    const std::vector<PairedKeyVerificationResult>& results) {
  bool all_success = true;
  for (const auto& result : results) {
    if (result == PairedKeyVerificationResult::kFail) return result;

    if (result != PairedKeyVerificationResult::kSuccess)
      all_success = false;
  }

  return all_success ? PairedKeyVerificationResult::kSuccess
                     : PairedKeyVerificationResult::kUnable;
}

bool PairedKeyVerificationRunner::RelaxRestrictToContactsIfNeeded() const {
  return share_target_.is_known &&
         (clock_->Now() - nearby_share_settings_->GetLastVisibilityTimestamp() <
          kRelaxAfterSetVisibilityTimeout);
}

}  // namespace sharing
}  // namespace nearby
