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

#ifndef CORE_INTERNAL_BASE_BANDWIDTH_UPGRADE_HANDLER_H_
#define CORE_INTERNAL_BASE_BANDWIDTH_UPGRADE_HANDLER_H_

#include <cstdint>
#include <map>

#include "core/internal/bandwidth_upgrade_handler.h"
#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_channel_manager.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/api/count_down_latch.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

namespace base_bandwidth_upgrade_handler {

class RevertRunnable;
class InitiateBandwidthUpgradeForEndpointRunnable;
class ProcessEndpointDisconnectionRunnable;
class ProcessBandwidthUpgradeNegotiationFrameRunnable;

}  // namespace base_bandwidth_upgrade_handler

// Base class for managing the upgrade of endpoints to a different medium for
// communication (from whatever they were previously using).
//
// <p>The sequencing of the upgrade protocol is as follows:
// <ul>
//   <li>Initiator sets up an upgrade path, sends
//       BANDWIDTH_UPGRADE_NEGOTIATION.UPGRADE_PATH_AVAILABLE to Responder over
//       the prior EndpointChannel.
//   <li>Responder joins the upgrade path, sends (without encryption)
//       BANDWIDTH_UPGRADE_NEGOTIATION.CLIENT_INTRODUCTION over the new
//       EndpointChannel, and sends
//       BANDWIDTH_UPGRADE_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL over the
//       prior EndpointChannel.
//   <li>Initiator receives BANDWIDTH_UPGRADE_NEGOTIATION.CLIENT_INTRODUCTION
//       over the newly-established EndpointChannel, and sends
//       BANDWIDTH_UPGRADE_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL over the
//       prior EndpointChannel.
//   <li>Both wait to receive
//       BANDWIDTH_UPGRADE_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL from the
//       other, and upon doing so, send
//       BANDWIDTH_UPGRADE_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL to each other
//   <li>Both then wait to receive
//       BANDWIDTH_UPGRADE_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL from the
//       other, and upon doing so, close the prior EndpointChannel.
// </ul>
class BaseBandwidthUpgradeHandler : public BandwidthUpgradeHandler {
 public:
  using Platform = platform::ImplementationPlatform;

  explicit BaseBandwidthUpgradeHandler(
      Ptr<EndpointChannelManager> endpoint_channel_manager);
  ~BaseBandwidthUpgradeHandler() override;

  void revert() override;
  void processEndpointDisconnection(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      Ptr<CountDownLatch> process_disconnection_barrier) override;
  // Initiates the bandwidth upgrade and sends an UPGRADE_PATH_AVAILABLE
  // OfflineFrame.
  void initiateBandwidthUpgradeForEndpoint(
      Ptr<ClientProxy<Platform> > client_proxy,
      const string& endpoint_id) override;
  void processBandwidthUpgradeNegotiationFrame(
      ConstPtr<BandwidthUpgradeNegotiationFrame> bandwidth_upgrade_negotiation,
      Ptr<ClientProxy<Platform> > to_client_proxy,
      const string& from_endpoint_id,
      proto::connections::Medium current_medium) override;

 protected:
  // Represents the incoming Socket the Initiator has gotten after initializing
  // its upgraded bandwidth medium.
  class IncomingSocketConnection {
   public:
    virtual ~IncomingSocketConnection() {}

    virtual string socketToString() = 0;
    virtual void closeSocket() = 0;
    // TODO(ahlee): Make sure to be careful with the ownership story of this.
    // Leaning towards releasing to the caller.
    virtual Ptr<EndpointChannel> getEndpointChannel() = 0;
  };

  // Called by the Initiator to setup the upgraded medium for this endpoint (if
  // that hasn't already been done), and returns a serialized UpgradePathInfo
  // that can be sent to the Responder.
  // TODO(ahlee): This will differ from the Java code (previously threw an
  // UpgradeException). Leaving the return type simple for the skeleton - I'll
  // switch to a pair if the result enum is needed.
  // @BandwidthUpgradeHandlerThread
  virtual ConstPtr<ByteArray> initializeUpgradedMediumForEndpoint(
      const string& endpoint_id) = 0;
  // Called to revert any state changed by the Initiator to setup the upgraded
  // medium for an endpoint.
  // @BandwidthUpgradeHandlerThread
  virtual void revertImpl() = 0;
  // Called by the Responder to setup the upgraded medium for this endpoint (if
  // that hasn't already been done) using the UpgradePathInfo sent by the
  // Initiator, and returns a new EndpointChannel for the upgraded medium.
  // @BandwidthUpgradeHandlerThread
  // TODO(ahlee): This will differ from the Java code (previously threw an
  // exception).
  virtual Ptr<EndpointChannel> createUpgradedEndpointChannel(
      const string& endpoint_id,
      ConstPtr<BandwidthUpgradeNegotiationFrame::UpgradePathInfo>
          upgrade_path_info) = 0;
  // Returns the upgrade medium of the BandwidthUpgradeHandler.
  // @BandwidthUpgradeHandlerThread
  virtual proto::connections::Medium getUpgradeMedium() = 0;

  Ptr<EndpointChannelManager> getEndpointChannelManager();
  // Common functionality to take an incoming connection and go through the
  // upgrade process.
  // @BandwidthUpgradeHandlerThread
  void onIncomingConnection(
      Ptr<IncomingSocketConnection> incoming_socket_connection);
  void runOnBandwidthUpgradeHandlerThread(Ptr<Runnable> runnable);

 private:
  friend class base_bandwidth_upgrade_handler::RevertRunnable;
  friend class base_bandwidth_upgrade_handler::
      InitiateBandwidthUpgradeForEndpointRunnable;
  friend class base_bandwidth_upgrade_handler::
      ProcessEndpointDisconnectionRunnable;
  friend class base_bandwidth_upgrade_handler::
      ProcessBandwidthUpgradeNegotiationFrameRunnable;

  void runUpgradeProtocol(Ptr<ClientProxy<Platform> > client_proxy,
                          const string& endpoint_id,
                          Ptr<EndpointChannel> new_endpoint_channel);
  void processBandwidthUpgradePathAvailableEvent(
      const string& endpoint_id, Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<BandwidthUpgradeNegotiationFrame::UpgradePathInfo>
          upgrade_path_info,
      proto::connections::Medium current_medium);
  Ptr<EndpointChannel> processBandwidthUpgradePathAvailableEventInternal(
      const string& endpoint_id, Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<BandwidthUpgradeNegotiationFrame::UpgradePathInfo>
          upgrade_path_info);
  void processLastWriteToPriorChannelEvent(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id);
  void processSafeToClosePriorChannelEvent(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id);
  std::int64_t calculateCloseDelay(const string& endpoint_id);
  std::int64_t getMillisSinceSafeCloseWritten(const string& endpoint_id);
  void attemptToRecordBandwidthUpgradeErrorForUnknownEndpoint(
      proto::connections::BandwidthUpgradeResult result,
      proto::connections::BandwidthUpgradeErrorStage error_stage);
  Ptr<BandwidthUpgradeNegotiationFrame::ClientIntroduction>
  readClientIntroductionFrame(Ptr<EndpointChannel> endpoint_channel);

  Ptr<EndpointChannelManager> endpoint_channel_manager_;
  ScopedPtr<Ptr<typename Platform::ScheduledExecutorType> > alarm_executor_;
  ScopedPtr<Ptr<typename Platform::SingleThreadExecutorType> > serial_executor_;
  // Stores each upgraded endpoint's previous EndpointChannel (that was
  // displaced in favor of a new EndpointChannel) temporarily, until it can
  // safely be shut down for good in processLastWriteToPriorChannelEvent().
  typedef std::map<string, Ptr<EndpointChannel> > PreviousEndpointChannelsMap;
  PreviousEndpointChannelsMap previous_endpoint_channels_;
  // Maps endpointId -> ClientProxy for which
  // initiateBandwidthUpgradeForEndpoint() has been called but which have not
  // yet completed the upgrade via onIncomingConnection().
  typedef std::map<string, Ptr<ClientProxy<Platform> > > InProgressUpgradesMap;
  InProgressUpgradesMap in_progress_upgrades_;
  // Maps endpointId -> timestamp of when the SAFE_TO_CLOSE message was written.
  typedef std::map<string, std::int64_t> SafeToCloseWriteTimestampsMap;
  SafeToCloseWriteTimestampsMap safe_to_close_write_timestamps_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_BASE_BANDWIDTH_UPGRADE_HANDLER_H_
