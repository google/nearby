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

#ifndef CORE_INTERNAL_BANDWIDTH_UPGRADE_MANAGER_H_
#define CORE_INTERNAL_BANDWIDTH_UPGRADE_MANAGER_H_

#include <map>

#include "core/internal/bandwidth_upgrade_handler.h"
#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/medium_manager.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/api/platform.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

// Manages all known {@link BandwidthUpgradeHandler} implementations, delegating
// operations to the appropriate one as per the parameters passed in.
class BandwidthUpgradeManager
    : public EndpointManager<
          platform::ImplementationPlatform>::IncomingOfflineFrameProcessor {
 public:
  using Platform = platform::ImplementationPlatform;

  BandwidthUpgradeManager(Ptr<MediumManager<Platform>> medium_manager,
                          Ptr<EndpointChannelManager> endpoint_channel_manager,
                          Ptr<EndpointManager<Platform>> endpoint_manager);
  ~BandwidthUpgradeManager() override;

  // This is the point on the initiator side where the
  // current_bandwidth_upgrade_handler_ is set.
  void initiateBandwidthUpgradeForEndpoint(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      proto::connections::Medium medium);
  // This is the point on the non-initiator side where the
  // current_bandwidth_upgrade_handler_ is set.
  // @EndpointManagerReaderThread
  void processIncomingOfflineFrame(
      ConstPtr<OfflineFrame> offline_frame, const string& from_endpoint_id,
      Ptr<ClientProxy<Platform> > to_client_proxy,
      proto::connections::Medium current_medium) override;
  // @EndpointManagerReaderThread
  void processEndpointDisconnection(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      Ptr<CountDownLatch> process_disconnection_barrier) override;

 private:
  bool setCurrentBandwidthUpgradeHandler(proto::connections::Medium medium);

  Ptr<EndpointManager<Platform> > endpoint_manager_;
  typedef std::map<proto::connections::Medium, Ptr<BandwidthUpgradeHandler>>
      BandwidthUpgradeHandlersMap;
  BandwidthUpgradeHandlersMap bandwidth_upgrade_handlers_;
  Ptr<BandwidthUpgradeHandler> current_bandwidth_upgrade_handler_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_BANDWIDTH_UPGRADE_MANAGER_H_
