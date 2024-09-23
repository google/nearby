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

#ifndef THIRD_PARTY_NEARBY_SHARING_SHARE_SESSION_H_
#define THIRD_PARTY_NEARBY_SHARING_SHARE_SESSION_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/clock.h"
#include "internal/platform/task_runner.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment_container.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/incoming_frames_reader.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/payload_tracker.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"

namespace nearby::sharing {

// Class that represents a single share session.
// This class is thread-compatible.
class ShareSession {
 public:
  ShareSession(TaskRunner& service_thread,
               analytics::AnalyticsRecorder& analytics_recorder,
               std::string endpoint_id, const ShareTarget& share_target);
  ShareSession(ShareSession&&);
  ShareSession& operator=(ShareSession&&) = delete;
  ShareSession& operator=(const ShareSession&) = delete;
  virtual ~ShareSession();

  virtual bool IsIncoming() const = 0;
  std::string endpoint_id() const { return endpoint_id_; }
  const std::optional<NearbyShareDecryptedPublicCertificate>& certificate()
      const {
    return certificate_;
  }

  void set_certificate(NearbyShareDecryptedPublicCertificate certificate) {
    certificate_ = std::move(certificate);
  }
  void clear_certificate() { certificate_ = std::nullopt; }

  NearbyConnection* connection() const { return connection_; }
  bool IsConnected() const { return connection_ != nullptr; }

  void UpdateTransferMetadata(const TransferMetadata& transfer_metadata);

  const std::string& token() const { return token_; }

  IncomingFramesReader* frames_reader() const { return frames_reader_.get(); }

  std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>
  payload_tracker() const;

  int64_t session_id() const { return session_id_; }

  void set_session_id(int64_t session_id) { session_id_ = session_id; }

  std::optional<absl::Time> connection_start_time() const {
    return connection_start_time_;
  }

  location::nearby::proto::sharing::OSType os_type() const { return os_type_; }

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
  bool OnConnected(absl::Time connect_start_time,
                   NearbyConnectionsManager* connections_manager,
                   NearbyConnection* connection);

  // Send TransferMetadataUpdate with the final status.
  // If connected, also close the connection.
  void Abort(TransferMetadata::Status status);

  void RunPairedKeyVerification(
      Clock* clock, location::nearby::proto::sharing::OSType os_type,
      const PairedKeyVerificationRunner::VisibilityHistory& visibility_history,
      NearbyShareCertificateManager* certificate_manager,
      const std::vector<uint8_t>& token,
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

  void CancelPayloads();

  const absl::flat_hash_map<int64_t, int64_t>& attachment_payload_map() const {
    return attachment_payload_map_;
  }

  void WriteResponseFrame(
      nearby::sharing::service::proto::ConnectionResponseFrame::Status
          response_status);
  void WriteCancelFrame();

  void SetTokenForTests(std::string token) { token_ = std::move(token); }

  void Disconnect();

 protected:
  virtual void InvokeTransferUpdateCallback(
      const TransferMetadata& metadata) = 0;
  virtual bool OnNewConnection(NearbyConnection* connection) = 0;
  virtual void OnConnectionDisconnected() {}

  analytics::AnalyticsRecorder& analytics_recorder() {
    return analytics_recorder_;
  };

  TaskRunner& service_thread() const { return service_thread_; }
  void SetAttachmentPayloadId(int64_t attachment_id, int64_t payload_id);

  void set_payload_tracker(std::shared_ptr<PayloadTracker> payload_tracker) {
    payload_tracker_ = std::move(payload_tracker);
  }

  AttachmentContainer& mutable_attachment_container() {
    return attachment_container_;
  }
  void WriteFrame(const nearby::sharing::service::proto::Frame& frame);
  // Processes the PairedKeyVerificationResult.
  // Returns true if verification was successful.
  bool HandleKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult result,
      location::nearby::proto::sharing::OSType share_target_os_type);

  NearbyConnectionsManager* connections_manager() {
    return connections_manager_;
  }
  void set_endpoint_id(absl::string_view endpoint_id) {
    endpoint_id_ = std::string(endpoint_id);
  }
  void set_share_target(const ShareTarget& share_target) {
    share_target_ = share_target;
    self_share_ = share_target.for_self_share;
  }

 private:
  TaskRunner& service_thread_;
  analytics::AnalyticsRecorder& analytics_recorder_;
  std::string endpoint_id_;
  std::optional<NearbyShareDecryptedPublicCertificate> certificate_;
  NearbyConnectionsManager* connections_manager_ = nullptr;
  NearbyConnection* connection_ = nullptr;
  // If not empty, this is the 4 digit token used to verify the connection.
  // If token is empty, it means self-share and verification is not needed.
  std::string token_;
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
  absl::flat_hash_map<int64_t, int64_t> attachment_payload_map_;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_SHARE_SESSION_H_
