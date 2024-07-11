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

#ifndef THIRD_PARTY_NEARBY_SHARING_INCOMING_SHARE_SESSION_H_
#define THIRD_PARTY_NEARBY_SHARING_INCOMING_SHARE_SESSION_H_

#include <cstdint>
#include <filesystem>  // NOLINT
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "internal/platform/clock.h"
#include "internal/platform/task_runner.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_session.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"

namespace nearby::sharing {

// Class that represents a single incoming share session.
// This class is thread-compatible.
class IncomingShareSession : public ShareSession {
 public:
  IncomingShareSession(
      TaskRunner& service_thread,
      analytics::AnalyticsRecorder& analytics_recorder, std::string endpoint_id,
      const ShareTarget& share_target,
      std::function<void(const IncomingShareSession&, const TransferMetadata&)>
          transfer_update_callback);
  IncomingShareSession(IncomingShareSession&&);
  ~IncomingShareSession() override;

  bool IsIncoming() const override { return true; }

  // Returns nullopt on success.
  // On failure, returns the status that should be used to terminate the
  // connection.
  std::optional<TransferMetadata::Status> ProcessIntroduction(
      const nearby::sharing::service::proto::IntroductionFrame&
          introduction_frame);

  // Processes the PairedKeyVerificationResult.
  // Returns true if verification was successful and the session is now waiting
  // for the introduction frame.  Calls |introduction_callback| when it is
  // received.
  bool ProcessKeyVerificationResult(
      PairedKeyVerificationRunner::PairedKeyVerificationResult result,
      location::nearby::proto::sharing::OSType share_target_os_type,
      std::function<void(
          std::optional<nearby::sharing::service::proto::IntroductionFrame>)>
          introduction_callback);

  // Update file attachment paths with payload paths.
  bool UpdateFilePayloadPaths(
      const NearbyConnectionsManager& connections_manager);

  // Accept the transfer and begin listening for payload transfer updates.
  void AcceptTransfer(
      Clock* clock, NearbyConnectionsManager& connections_manager,
      std::function<void(int64_t, TransferMetadata)> update_callback);

  // Once transfer has completed, make payload content available in the
  // corresponding Attachment.
  // Returns true if all payloads were successfully finalized.
  bool FinalizePayloads(const NearbyConnectionsManager& connections_manager);

  // Returns the file paths of all file payloads.
  std::vector<std::filesystem::path> GetPayloadFilePaths() const;

  // Upgrade bandwidth if it is needed.
  // Returns true if bandwidth upgrade was requested.
  bool TryUpgradeBandwidth(NearbyConnectionsManager& connections_manager);

 protected:
  void InvokeTransferUpdateCallback(const TransferMetadata& metadata) override;
  bool OnNewConnection(NearbyConnection* connection) override;

 private:
  // Copy payload contents from the NearbyConnection to the Attachment.
  bool UpdatePayloadContents(
      const NearbyConnectionsManager& connections_manager);

  std::function<void(const IncomingShareSession&, const TransferMetadata&)>
      transfer_update_callback_;

  bool bandwidth_upgrade_requested_ = false;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_INCOMING_SHARE_SESSION_H_
