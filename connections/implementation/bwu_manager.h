// Copyright 2021 Google LLC
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

#ifndef CORE_INTERNAL_BWU_MANAGER_H_
#define CORE_INTERNAL_BWU_MANAGER_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/mediums/mediums.h"
#include "internal/platform/scheduled_executor.h"

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

  ~BwuManager() override;

  // Initiates the bandwidth upgrade and sends an UPGRADE_PATH_AVAILABLE
  // frame to the remote device (Responder). If |new_medium| is not provided,
  // the best available medium is chosen.
  void InitiateBwuForEndpoint(ClientProxy* client_proxy,
                              const std::string& endpoint_id,
                              Medium new_medium = Medium::UNKNOWN_MEDIUM);

  // == EndpointManager::FrameProcessor interface ==.
  // This is also an entry point for handling messages for both outbound and
  // inbound BWU protocol.
  // @EndpointManagerReaderThread
  void OnIncomingFrame(location::nearby::connections::OfflineFrame& frame,
                       const std::string& endpoint_id, ClientProxy* client,
                       Medium medium,
                       PacketMetaData& packet_meta_data) override;

  // Cleans up in-progress upgrades after endpoint disconnection.
  // @EndpointManagerReaderThread
  void OnEndpointDisconnect(ClientProxy* client_proxy,
                            const std::string& service_id,
                            const std::string& endpoint_id,
                            CountDownLatch barrier) override;

  void Shutdown();

  // After this is called, all tasks intended for the serial executor are
  // invoked immediately on the calling thread.
  void MakeSingleThreadedForTesting();

  // OnIncomingConnection is passed to the handlers during handler creation in
  // InitBwuHandlers. When explicitly passing handlers into the BwuManager
  // constructor, we need a way to invoke OnIncomingConnection.
  void InvokeOnIncomingConnectionForTesting(
      ClientProxy* client,
      std::unique_ptr<BwuHandler::IncomingSocketConnection> mutable_connection);

  // Shutdown the executors during Nearby Connections shutdown process before
  // Core objects destruction so that no task will be run/posted after
  // ClientProxy objects are deleted.
  void ShutdownExecutors();

  // Check if BWU is on going for a specific Endpoint
  bool IsUpgradeOngoing(const std::string& endpoint_id);

 private:
  static constexpr absl::Duration kReadClientIntroductionFrameTimeout =
      absl::Seconds(5);

  void InitBwuHandlers();
  void RunOnBwuManagerThread(const std::string& name, Runnable runnable);
  std::vector<Medium> StripOutUnavailableMediums(
      const std::vector<Medium>& mediums) const;
  Medium ChooseBestUpgradeMedium(const std::string& endpoint_id,
                                 const std::vector<Medium>& mediums) const;

  // BaseBwuHandler
  using ClientIntroduction = BwuNegotiationFrame::ClientIntroduction;

  // Processes the BwuNegotiationFrames that come over the EndpointChannel on
  // both initiator and responder side of the upgrade.
  void OnBwuNegotiationFrame(ClientProxy* client,
                             const BwuNegotiationFrame frame,
                             const string& endpoint_id);

  // Called to revert any state changed in the course of setting up the upgraded
  // medium for an endpoint.
  void RevertBwuMediumForEndpoint(const std::string& service_id,
                                  const std::string& endpoint_id);

  // Get/Set the currently selected upgrade medium for this endpoint, or
  // UNKNOWN_MEDIUM if nothing is selected. This is the medium we are attempting
  // to upgrade to, not necessarily the endpoint's current connection medium.
  Medium GetBwuMediumForEndpoint(const std::string& endpoint_id) const;
  void SetBwuMediumForEndpoint(const std::string& endpoint_id, Medium medium);

  BwuHandler* GetHandlerForMedium(Medium medium) const;

  // Common functionality to take an incoming connection and go through the
  // upgrade process. This is a callback, invoked by concrete handlers, once
  // connection is available.
  void OnIncomingConnection(
      ClientProxy* client,
      std::unique_ptr<BwuHandler::IncomingSocketConnection> mutable_connection);

  void RunUpgradeProtocol(ClientProxy* client, const std::string& endpoint_id,
                          std::unique_ptr<EndpointChannel> new_channel,
                          bool enable_encryption);
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
  bool ReadClientIntroductionAckFrame(EndpointChannel* endpoint_channel);
  bool WriteClientIntroductionAckFrame(EndpointChannel* endpoint_channel);
  void ProcessEndpointDisconnection(ClientProxy* client,
                                    const std::string& endpoint_id,
                                    CountDownLatch* barrier);
  void ProcessUpgradeFailureEvent(ClientProxy* client,
                                  const std::string& endpoint_id,
                                  const UpgradePathInfo& upgrade_info);
  void CancelRetryUpgradeAlarm(const std::string& endpoint_id);
  void CancelAllRetryUpgradeAlarms();
  void TryNextBestUpgradeMediums(ClientProxy* client,
                                 const std::string& endpoint_id,
                                 std::vector<Medium> upgrade_mediums);
  absl::Duration CalculateNextRetryDelay(const std::string& endpoint_id);
  void RetryUpgradesAfterDelay(ClientProxy* client,
                               const std::string& endpoint_id);
  void AttemptToRecordBandwidthUpgradeErrorForUnknownEndpoint(
      location::nearby::proto::connections::BandwidthUpgradeResult result,
      location::nearby::proto::connections::BandwidthUpgradeErrorStage
          error_stage);

  bool is_single_threaded_for_testing_ = false;

  Config config_;

  // The current bandwidth upgrade medium.
  // Only used if feature flag support_multiple_bwu_mediums is DISABLED.
  Medium medium_ = Medium::UNKNOWN_MEDIUM;
  // Map from endpoint ID to the bandwidth upgrade medium it is trying to
  // upgrade to or is currently using until reverted. Only used if feature flag
  // support_multiple_bwu_mediums is ENABLED.
  absl::flat_hash_map<std::string, Medium> endpoint_id_to_bwu_medium_;

  Mediums* mediums_;
  absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers_;

  EndpointManager* endpoint_manager_;
  EndpointChannelManager* channel_manager_;
  ScheduledExecutor alarm_executor_;
  SingleThreadExecutor serial_executor_;
  // Stores each upgraded endpoint's previous EndpointChannel (that was
  // displaced in favor of a new EndpointChannel) temporarily, until it can
  // safely be shut down for good in processLastWriteToPriorChannelEvent().
  absl::flat_hash_map<std::string, std::shared_ptr<EndpointChannel>>
      previous_endpoint_channels_;
  absl::flat_hash_set<std::string> successfully_upgraded_endpoints_;
  // Maps endpointId -> ClientProxy for which
  // initiateBwuForEndpoint() has been called but which have not
  // yet completed the upgrade via onIncomingConnection().
  absl::flat_hash_map<std::string, ClientProxy*> in_progress_upgrades_;
  // Maps endpointId -> timestamp of when the SAFE_TO_CLOSE message was written.
  absl::flat_hash_map<std::string, absl::Time> safe_to_close_write_timestamps_;
  absl::flat_hash_map<
      std::string, std::pair<std::unique_ptr<CancelableAlarm>, absl::Duration>>
      retry_upgrade_alarms_;
  // Maps endpointId -> duration of delay before bwu retry.
  // When bwu failed, retry_upgrade_alarms_ will clear the entry before the
  // retry happen, then we can not find the last delay used in the alarm. Thus
  // using a different map to keep track of the delays per endpoint.
  absl::flat_hash_map<std::string, absl::Duration> retry_delays_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_BWU_MANAGER_H_
