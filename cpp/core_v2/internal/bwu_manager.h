// Copyright 2020 Google LLC
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

#ifndef CORE_V2_INTERNAL_BWU_MANAGER_H_
#define CORE_V2_INTERNAL_BWU_MANAGER_H_

#include <string>
#include <vector>

#include "core_v2/internal/bwu_handler.h"
#include "core_v2/internal/client_proxy.h"
#include "core_v2/internal/endpoint_manager.h"
#include "core_v2/internal/mediums/mediums.h"
#include "core_v2/options.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/public/scheduled_executor.h"
#include "proto/connections_enums.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace connections {

// Base class for managing the upgrade of endpoints to a different medium for
// communication (from whatever they were previously using).
//
// The sequencing of the upgrade protocol is as follows:
//   - Initiator sets up an upgrade path, sends
//     BANDWIDTH_UPGRADE_NEGOTIATION.UPGRADE_PATH_AVAILABLE to Responder over
//     the prior EndpointChannel.
//   - Responder joins the upgrade path, sends (possibly without encryption)
//     BANDWIDTH_UPGRADE_NEGOTIATION.CLIENT_INTRODUCTION over the new
//     EndpointChannel, and sends
//     BANDWIDTH_UPGRADE_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL over the
//     prior EndpointChannel.
//   - Initiator receives BANDWIDTH_UPGRADE_NEGOTIATION.CLIENT_INTRODUCTION
//     over the newly-established EndpointChannel, and sends
//     BANDWIDTH_UPGRADE_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL over the
//     prior EndpointChannel.
//   - Both wait to receive
//     BANDWIDTH_UPGRADE_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL from the
//     other, and upon doing so, send
//     BANDWIDTH_UPGRADE_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL to each other
//   - Both then wait to receive
//     BANDWIDTH_UPGRADE_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL from the
//     other, and upon doing so, close the prior EndpointChannel.
class BwuManager : public EndpointManager::FrameProcessor {
 public:
  using UpgradePathInfo = BwuHandler::UpgradePathInfo;

  struct Config {
    BooleanMediumSelector allow_upgrade_to;
    absl::Duration bandwidth_upgrade_retry_delay;
    absl::Duration bandwidth_upgrade_retry_max_delay;
  };

  BwuManager(Mediums& mediums, EndpointManager& endpoint_manager,
             EndpointChannelManager& channel_manager,
             absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers,
             Config config);

  ~BwuManager() override = default;

  // This is the point on the outbound BWU protocol where the handler_ is set.
  // Function initiates the bandwidth upgrade and sends an
  // UPGRADE_PATH_AVAILABLE OfflineFrame.
  void InitiateBwuForEndpoint(ClientProxy* client_proxy,
                              const std::string& endpoint_id,
                              Medium new_medium = Medium::UNKNOWN_MEDIUM);

  // == EndpointManager::FrameProcessor interface ==.
  // This is the point on the inbound BWU protocol where the handler_ is set.
  // This is also an entry point for handling messages for both outbound and
  // inbound BWU protocol.
  // @EndpointManagerReaderThread
  void OnIncomingFrame(OfflineFrame& frame, const std::string& endpoint_id,
                       ClientProxy* client, Medium medium) override;

  // Cleans up in-progress upgrades after endpoint disconnection.
  // @EndpointManagerReaderThread
  void OnEndpointDisconnect(ClientProxy* client_proxy,
                            const std::string& endpoint_id,
                            CountDownLatch* barrier) override;
  void Shutdown();

 private:
  BwuHandler* SetCurrentBwuHandler(Medium medium);
  void InitBwuHandlers();
  void RunOnBwuManagerThread(std::function<void()> runnable);
  std::vector<Medium> StripOutUnavailableMediums(
      const std::vector<Medium>& mediums);
  Medium ChooseBestUpgradeMedium(const std::vector<Medium>& mediums);

  // BaseBwuHandler
  using ClientIntroduction = BwuNegotiationFrame::ClientIntroduction;

  // Processes the BwuNegotiationFrames that come over the
  // EndpointChannel on both initiator and responder side of the upgrade.
  void OnBwuNegotiationFrame(ClientProxy* client,
                             const BwuNegotiationFrame& frame,
                             const string& endpoint_id);

  // Called to revert any state changed by the Initiator or Responder in the
  // course of setting up the upgraded medium for an endpoint.
  void Revert();

  // Common functionality to take an incoming connection and go through the
  // upgrade process. This is a callback, invoked by concrete handlers, once
  // connection is available.
  void OnIncomingConnection(ClientProxy* client,
                            BwuHandler::IncomingSocketConnection* connection);

  void RunUpgradeProtocol(ClientProxy* client, const std::string& endpoint_id,
                          std::unique_ptr<EndpointChannel> new_channel);
  void RunUpgradeFailedProtocol(ClientProxy* client,
                                const std::string& endpoint_id,
                                const UpgradePathInfo& upgrade_path_info);
  void ProcessBwuPathAvailableEvent(ClientProxy* client,
                                    const std::string& endpoint_id,
                                    const UpgradePathInfo& upgrade_path_info);
  std::unique_ptr<EndpointChannel> ProcessBwuPathAvailableEventInternal(
      ClientProxy* client, const std::string& endpoint_id,
      const UpgradePathInfo& upgrade_path_info);
  void ProcessLastWriteToPriorChannelEvent(ClientProxy* client,
                                           const std::string& endpoint_id);
  void ProcessSafeToClosePriorChannelEvent(ClientProxy* client,
                                           const std::string& endpoint_id);
  bool ReadClientIntroductionFrame(EndpointChannel* endpoint_channel,
                                   ClientIntroduction& introduction);
  void ProcessEndpointDisconnection(ClientProxy* client,
                                    const std::string& endpoint_id,
                                    CountDownLatch* barrier);
  void ProcessUpgradeFailureEvent(ClientProxy* client,
                                  const std::string& endpoint_id,
                                  const UpgradePathInfo& upgrade_info);
  void CancelRetryUpgradeAlarm(const std::string& endpoint_id);
  void CancelAllRetryUpgradeAlarms();
  void RetryUpgradeMediums(ClientProxy* client, const std::string& endpoint_id,
                           std::vector<Medium> upgrade_mediums);
  Medium GetEndpointMedium(const std::string& endpoint_id);
  absl::Duration CalculateNextRetryDelay(const std::string& endpoint_id);
  void RetryUpgradesAfterDelay(ClientProxy* client,
                               const std::string& endpoint_id);

  Config config_;

  Medium medium_ = Medium::UNKNOWN_MEDIUM;
  BwuHandler* handler_ = nullptr;
  Mediums* mediums_;
  absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers_;

  EndpointManager* endpoint_manager_;
  EndpointChannelManager* channel_manager_;
  ScheduledExecutor alarm_executor_;
  SingleThreadExecutor serial_executor_;
  // Stores each upgraded endpoint's previous EndpointChannel (that was
  // displaced in favor of a new EndpointChannel) temporarily, until it can
  // safely be shut down for good in processLastWriteToPriorChannelEvent().
  absl::flat_hash_map<std::string, std::unique_ptr<EndpointChannel>>
      previous_endpoint_channels_;
  absl::flat_hash_map<std::string, std::shared_ptr<EndpointChannel>>
      old_channels_;
  absl::flat_hash_set<std::string> successfully_upgraded_endpoints_;
  // Maps endpointId -> ClientProxy for which
  // initiateBwuForEndpoint() has been called but which have not
  // yet completed the upgrade via onIncomingConnection().
  absl::flat_hash_map<std::string, ClientProxy*> in_progress_upgrades_;
  // Maps endpointId -> timestamp of when the SAFE_TO_CLOSE message was written.
  absl::flat_hash_map<std::string, absl::Time> safe_to_close_write_timestamps_;
  absl::flat_hash_map<std::string, std::pair<CancelableAlarm, absl::Duration>>
      retry_upgrade_alarms_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_BWU_MANAGER_H_
