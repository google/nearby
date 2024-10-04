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
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/nearby_file_handler.h"
#include "sharing/paired_key_verification_runner.h"
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
      TaskRunner& service_thread,
      analytics::AnalyticsRecorder& analytics_recorder, std::string endpoint_id,
      const ShareTarget& share_target,
      std::function<void(OutgoingShareSession&, const TransferMetadata&)>
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

  Status connection_layer_status() const { return connection_layer_status_; }

  void set_connection_layer_status(Status status) {
    connection_layer_status_ = status;
  }

  bool ProcessKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult result,
      location::nearby::proto::sharing::OSType share_target_os_type);

  std::vector<std::filesystem::path> GetFilePaths() const;

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
  // `update_callback`.
  // Any other frames received will be passed to `frame_read_callback`.
  void SendPayloads(
      bool enable_transfer_cancellation_optimization, Clock* clock,
      std::function<
          void(std::optional<nearby::sharing::service::proto::V1Frame> frame)>
          frame_read_callback,
      std::function<void(int64_t, TransferMetadata)> update_callback);
  // Send the next payload to NearbyConnectionManager.
  // Used only if enable_transfer_cancellation_optimization is true.
  void SendNextPayload();

  // Cache the kComplete metadata in pending_complete_metadata_ and forward a
  // modified copy that changes kComplete into kInProgress.
  void DelayCompleteMetadata(const TransferMetadata& complete_metadata);
  // Disconnect timeout expired without receiving client disconnect.
  void DisconnectionTimeout();
  // Used only for OutgoingShareSession De-duplication.
  void UpdateSessionForDedup(
      const ShareTarget& share_target,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate,
      absl::string_view endpoint_id);

 protected:
  void InvokeTransferUpdateCallback(const TransferMetadata& metadata) override;
  bool OnNewConnection(NearbyConnection* connection) override;
  void OnConnectionDisconnected() override;

 private:
  // Create a payload status listener to send status change to
  // `update_callback`.  Send all payloads to NearbyConnectionManager.
  void SendAllPayloads(
      Clock* clock,
      std::function<void(int64_t, TransferMetadata)> update_callback);

  // Create a payload status listener to send status change to
  // `update_callback`.
  void InitSendPayload(
      Clock* clock,
      std::function<void(int64_t, TransferMetadata)> update_callback);

  std::vector<Payload> ExtractTextPayloads();
  std::vector<Payload> ExtractFilePayloads();
  std::vector<Payload> ExtractWifiCredentialsPayloads();
  std::optional<Payload> ExtractNextPayload();
  bool FillIntroductionFrame(
      nearby::sharing::service::proto::IntroductionFrame* introduction) const;


  std::optional<std::string> obfuscated_gaia_id_;
  // All payloads are in the same order as the attachments in the share target.
  std::vector<Payload> text_payloads_;
  std::vector<Payload> file_payloads_;
  std::vector<Payload> wifi_credentials_payloads_;
  Status connection_layer_status_;
  std::function<void(OutgoingShareSession&, const TransferMetadata&)>
      transfer_update_callback_;
  bool ready_for_accept_ = false;
  // This alarm is used to disconnect the sharing connection if both sides do
  // not press accept within the timeout.
  std::unique_ptr<ThreadTimer> mutual_acceptance_timeout_;
  std::optional<TransferMetadata> pending_complete_metadata_;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_OUTGOING_SHARE_SESSION_H_
