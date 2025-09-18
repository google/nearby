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

#ifndef THIRD_PARTY_NEARBY_SHARING_OUTGOING_SHARE_SESSION_H_
#define THIRD_PARTY_NEARBY_SHARING_OUTGOING_SHARE_SESSION_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/base/file_path.h"
#include "internal/platform/clock.h"
#include "internal/platform/task_runner.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment_container.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/nearby_file_handler.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/share_session.h"
#include "sharing/share_target.h"
#include "sharing/thread_timer.h"
#include "sharing/transfer_metadata.h"

namespace nearby::sharing {

// Class that represents a single outgoing share session.
// This class is thread-compatible.
class OutgoingShareSession : public ShareSession {
 public:
  OutgoingShareSession(
      Clock* clock, TaskRunner& service_thread,
      NearbyConnectionsManager* connections_manager,
      analytics::AnalyticsRecorder& analytics_recorder, std::string endpoint_id,
      const ShareTarget& share_target,
      absl::AnyInvocable<void(OutgoingShareSession&, const TransferMetadata&)>
          transfer_update_callback);
  OutgoingShareSession(OutgoingShareSession&&);
  ~OutgoingShareSession() override;

  bool IsIncoming() const override { return false; }

  const std::optional<std::string>& obfuscated_gaia_id() const {
    return obfuscated_gaia_id_;
  }

  void set_obfuscated_gaia_id(std::string obfuscated_gaia_id) {
    obfuscated_gaia_id_ = std::move(obfuscated_gaia_id);
  }

  const std::vector<Payload>& text_payloads() const { return text_payloads_; }

  const std::vector<Payload>& wifi_credentials_payloads() const {
    return wifi_credentials_payloads_;
  }

  const std::vector<Payload>& file_payloads() const { return file_payloads_; }

  void InitiateSendAttachments(
      std::unique_ptr<AttachmentContainer> attachment_container);

  bool ProcessKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult result,
      location::nearby::proto::sharing::OSType share_target_os_type);

  std::vector<FilePath> GetFilePaths() const;

  void CreateTextPayloads();
  void CreateWifiCredentialsPayloads();
  // Create file payloads and update the file size of all file attachments.
  // The list of file infos must be sorted in the same order as the file
  // attachments in the share target.
  // Returns true if all file payloads are created successfully.
  bool CreateFilePayloads(
      const std::vector<NearbyFileHandler::FileInfo>& files);

  // Returns true if the introduction frame is written successfully.
  // `timeout_callback` is called if accept is not received from both sender and
  // receiver within the timeout.
  bool SendIntroduction(std::function<void()> timeout_callback);

  // Accept the outgoing transfer and wait for remote accept message in a
  // ConnectionResponseFrame.
  bool AcceptTransfer(
      std::function<
          void(std::optional<
               nearby::sharing::service::proto::ConnectionResponseFrame>)>
          response_callback);

  // Process the ConnectionResponseFrame.
  // On success, returns std::nullopt.
  // On failure, returns the status if the connection should be aborted.
  std::optional<TransferMetadata::Status> HandleConnectionResponse(
      std::optional<nearby::sharing::service::proto::ConnectionResponseFrame>
          response);

  // Begin sending payloads.
  // Listen to the payload status change and send the status to
  // `payload_transder_update_callback`.
  // Any other frames received will be passed to `frame_read_callback`.
  void SendPayloads(
      std::function<
          void(std::optional<nearby::sharing::service::proto::V1Frame> frame)>
          frame_read_callback,
      std::function<void()> payload_transder_update_callback);
  // Send the next payload to NearbyConnectionManager.
  // Used only if enable_transfer_cancellation_optimization is true.
  void SendNextPayload();

  // Called when all payloads have been sent.
  void SendAttachmentsCompleted(const TransferMetadata& metadata);

  // Wait for receiver to close connection before we notify that the transfer
  // has succeeded.  Here we delay the `complete_metadata` update until receiver
  // disconnects and forward a modified copy that changes kComplete into
  // kInProgress.
  // A 1 min timer is setup so that if we do not receive disconnect from
  // receiver, we assume the transfer has failed.
  void DelayComplete(const TransferMetadata& complete_metadata);
  // Updates the share target in the session to `share_target`.
  // If the session is not connected, also updates the `certificate` and
  // `endpoint_id`.
  // Returns true if the session is not connected and updated successfully.
  bool UpdateSessionForDedup(
      const ShareTarget& share_target,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate,
      absl::string_view endpoint_id);

  // Establish a connection to the remote device identified by `endpoint_info`.
  // `callback` is called when with the connection establishment status..
  void Connect(std::vector<uint8_t> endpoint_info,
               std::optional<std::vector<uint8_t>> bluetooth_mac_address,
               nearby::sharing::proto::DataUsage data_usage,
               bool disable_wifi_hotspot,
               std::function<void(absl::string_view endpoint_id,
                                  NearbyConnection* connection, Status status)>
                   callback);

  // Called to process the result of a connection attempt.
  // Returns true if the connection was successful.
  bool OnConnectResult(NearbyConnection* connection, Status status);

  std::optional<TransferMetadata> ProcessPayloadTransferUpdates();

  void SetAdvancedProtectionStatus(bool advanced_protection_enabled,
                                   bool advanced_protection_mismatch) {
    advanced_protection_enabled_ = advanced_protection_enabled;
    advanced_protection_mismatch_ = advanced_protection_mismatch;
  }

 protected:
  void InvokeTransferUpdateCallback(const TransferMetadata& metadata) override;
  void OnConnectionDisconnected() override;

 private:
  // Calculates transport type based on attachment size.
  TransportType GetTransportType(bool disable_wifi_hotspot) const;

  std::optional<Payload> ExtractNextPayload();
  bool FillIntroductionFrame(
      nearby::sharing::service::proto::IntroductionFrame* introduction) const;

  std::optional<std::string> obfuscated_gaia_id_;
  // All payloads are in the same order as the attachments in the share target.
  std::vector<Payload> text_payloads_;
  std::vector<Payload> file_payloads_;
  std::vector<Payload> wifi_credentials_payloads_;
  Status connection_layer_status_ = Status::kUnknown;
  absl::AnyInvocable<void(OutgoingShareSession&, const TransferMetadata&)>
      transfer_update_callback_;
  bool ready_for_accept_ = false;
  // This alarm is used to disconnect the sharing connection if both sides do
  // not press accept within the timeout.
  std::unique_ptr<ThreadTimer> mutual_acceptance_timeout_;
  std::optional<TransferMetadata> pending_complete_metadata_;
  absl::Time connection_start_time_;
  // Timeout waiting for remote disconnect in order to complete transfer.
  std::unique_ptr<ThreadTimer> disconnection_timeout_;
  bool advanced_protection_enabled_ = false;
  bool advanced_protection_mismatch_ = false;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_OUTGOING_SHARE_SESSION_H_
