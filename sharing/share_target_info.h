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

#ifndef THIRD_PARTY_NEARBY_SHARING_SHARE_TARGET_INFO_H_
#define THIRD_PARTY_NEARBY_SHARING_SHARE_TARGET_INFO_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "internal/platform/implementation/device_info.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/attachment_container.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/incoming_frames_reader.h"
#include "sharing/internal/public/context.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_sharing_decoder.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/payload_tracker.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"

namespace nearby {
namespace sharing {

// Additional information about the connection to a remote device.
class ShareTargetInfo {
 public:
  ShareTargetInfo(
      std::string endpoint_id, const ShareTarget& share_target);
  ShareTargetInfo(ShareTargetInfo&&);
  ShareTargetInfo& operator=(ShareTargetInfo&&);
  virtual ~ShareTargetInfo();

  virtual bool IsIncoming() const = 0;
  std::string endpoint_id() const { return endpoint_id_; }

  const std::optional<NearbyShareDecryptedPublicCertificate>& certificate()
      const {
    return certificate_;
  }

  void set_certificate(NearbyShareDecryptedPublicCertificate certificate) {
    certificate_ = std::move(certificate);
  }

  NearbyConnection* connection() const { return connection_; }

  void UpdateTransferMetadata(const TransferMetadata& transfer_metadata);

  const std::optional<std::string>& token() const { return token_; }

  void set_token(std::string token) { token_ = std::move(token); }

  IncomingFramesReader* frames_reader() const { return frames_reader_.get(); }

  std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>
  payload_tracker() const {
    return payload_tracker_->GetWeakPtr();
  }

  void set_payload_tracker(std::shared_ptr<PayloadTracker> payload_tracker) {
    payload_tracker_ = std::move(payload_tracker);
  }

  int64_t session_id() const { return session_id_; }

  void set_session_id(int64_t session_id) { session_id_ = session_id; }

  std::optional<absl::Time> connection_start_time() const {
    return connection_start_time_;
  }

  ::location::nearby::proto::sharing::OSType os_type() const {
    return os_type_;
  }

  void set_os_type(::location::nearby::proto::sharing::OSType os_type) {
    os_type_ = os_type;
  }

  bool self_share() const { return self_share_; }

  const ShareTarget& share_target() const { return share_target_; }

  // Sets the status to send in the TransferMetadataUpdate on connection
  // disconnect. If |status| is kUnknown, then no TransferMetadataUpdate will be
  // sent.  If |status| is set, it must be a final status.
  void set_disconnect_status(TransferMetadata::Status disconnect_status);

  TransferMetadata::Status disconnect_status() const {
    return disconnect_status_;
  }
  // Notifies the ShareTargetInfo that the connection has been established.
  // Returns true if the connection was successfully established.
  bool OnConnected(absl::Time connect_start_time, NearbyConnection* connection);

  void RunPairedKeyVerification(
      Context* context, NearbySharingDecoder* decoder,
      nearby::api::DeviceInfo::OsType os_type,
      const PairedKeyVerificationRunner::VisibilityHistory& visibility_history,
      NearbyShareCertificateManager* certificate_manager,
      std::optional<std::vector<uint8_t>> token,
      std::function<
          void(PairedKeyVerificationRunner::PairedKeyVerificationResult,
               location::nearby::proto::sharing::OSType)>
          callback);

  void OnDisconnect();
  void SetAttachmentContainer(AttachmentContainer container) {
    attachment_container_ = std::move(container);
  }
  const AttachmentContainer& attachment_container() const {
    return attachment_container_;
  }

  AttachmentContainer& mutable_attachment_container() {
    return attachment_container_;
  }

 protected:
  virtual void InvokeTransferUpdateCallback(
      const TransferMetadata& metadata) = 0;
  virtual bool OnNewConnection(NearbyConnection* connection) = 0;

 private:
  std::string endpoint_id_;
  std::optional<NearbyShareDecryptedPublicCertificate> certificate_;
  NearbyConnection* connection_ = nullptr;
  std::optional<std::string> token_;
  std::shared_ptr<IncomingFramesReader> frames_reader_;
  std::shared_ptr<PairedKeyVerificationRunner> key_verification_runner_;
  std::shared_ptr<PayloadTracker> payload_tracker_;
  int64_t session_id_;
  std::optional<absl::Time> connection_start_time_;
  ::location::nearby::proto::sharing::OSType os_type_ =
      ::location::nearby::proto::sharing::OSType::UNKNOWN_OS_TYPE;
  bool self_share_ = false;
  ShareTarget share_target_;
  bool got_final_status_ = false;
  // The status sent in the TransferMetadataUpdate on connection disconnect.
  // If status is kUnknown, then no TransferMetadataUpdate will be sent.
  TransferMetadata::Status disconnect_status_ =
      TransferMetadata::Status::kUnknown;
  AttachmentContainer attachment_container_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_SHARE_TARGET_INFO_H_
