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

#ifndef THIRD_PARTY_NEARBY_SHARING_PAIRED_KEY_VERIFICATION_RUNNER_H_
#define THIRD_PARTY_NEARBY_SHARING_PAIRED_KEY_VERIFICATION_RUNNER_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "absl/time/time.h"
#include "internal/platform/clock.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/incoming_frames_reader.h"
#include "sharing/nearby_connection.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/wire_format.pb.h"

namespace nearby {
namespace sharing {

class PairedKeyVerificationRunner
    : public std::enable_shared_from_this<PairedKeyVerificationRunner> {
 public:
  enum class PairedKeyVerificationResult {
    // Default value for verification result.
    kUnknown,
    // Succeeded with verification.
    kSuccess,
    // Failed to verify.
    kFail,
    // Unable to verify. Occurs when missing proper certificates.
    kUnable,
  };

  struct VisibilityHistory {
    proto::DeviceVisibility visibility;
    proto::DeviceVisibility last_visibility;
    absl::Time last_visibility_time;
  };

  PairedKeyVerificationRunner(
      Clock* clock, location::nearby::proto::sharing::OSType os_type,
      bool share_target_is_incoming,
      const VisibilityHistory& visibility_history,
      const std::vector<uint8_t>& token, NearbyConnection* connection,
      const std::optional<NearbyShareDecryptedPublicCertificate>& certificate,
      NearbyShareCertificateManager* certificate_manager,
      IncomingFramesReader* frames_reader, absl::Duration read_frame_timeout);

  ~PairedKeyVerificationRunner();

  void Run(std::function<
           void(PairedKeyVerificationResult verification_result,
                ::location::nearby::proto::sharing::OSType remote_os_type)>
               callback);

  std::weak_ptr<PairedKeyVerificationRunner> GetWeakPtr() {
    return this->weak_from_this();
  }

 private:
  void SendPairedKeyEncryptionFrame();
  void OnReadPairedKeyEncryptionFrame(
      std::optional<nearby::sharing::service::proto::V1Frame> frame);
  void OnReadPairedKeyResultFrame(
      std::optional<nearby::sharing::service::proto::V1Frame> frame);
  void SendPairedKeyResultFrame(PairedKeyVerificationResult result);
  // Verifies auth token hash in frame using private certificate for visibility.
  // Returns either kSuccess or kUnable.  This function never returns kFail.
  PairedKeyVerificationResult VerifyAuthTokenHashWithPrivateCertificate(
      proto::DeviceVisibility visibility,
      const nearby::sharing::service::proto::V1Frame& frame);
  PairedKeyVerificationResult VerifyPairedKeyEncryptionFrame(
      const nearby::sharing::service::proto::V1Frame& frame);
  void ApplyResult(PairedKeyVerificationResult result);
  // True if visibility has changed recently.
  bool IsVisibilityRecentlyUpdated() const;

  nearby::Clock* const clock_;
  const location::nearby::proto::sharing::OSType os_type_;
  VisibilityHistory visibility_history_;
  std::vector<uint8_t> raw_token_;
  NearbyConnection* connection_;
  std::optional<NearbyShareDecryptedPublicCertificate> certificate_;
  NearbyShareCertificateManager* certificate_manager_;
  IncomingFramesReader* frames_reader_;
  const absl::Duration read_frame_timeout_;
  std::function<void(PairedKeyVerificationResult,
                     ::location::nearby::proto::sharing::OSType)>
      callback_;
  PairedKeyVerificationResult verification_result_;
  char local_prefix_;
  char remote_prefix_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_PAIRED_KEY_VERIFICATION_RUNNER_H_
