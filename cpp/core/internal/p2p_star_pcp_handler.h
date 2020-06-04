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

#ifndef CORE_INTERNAL_P2P_STAR_PCP_HANDLER_H_
#define CORE_INTERNAL_P2P_STAR_PCP_HANDLER_H_

#include <vector>

#include "core/internal/bandwidth_upgrade_manager.h"
#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/medium_manager.h"
#include "core/internal/p2p_cluster_pcp_handler.h"
#include "core/internal/pcp.h"
#include "core/strategy.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

// Concrete implementation of the PCPHandler for the P2P_STAR PCP. This PCP is
// for mediums that have one server with (potentially) many clients; all mediums
// in P2P_CLUSTER are valid for P2P_STAR, but not all mediums in P2P_STAR and
// valid for P2P_CLUSTER.
//
// <p>Currently, this implementation advertises/discovers over BLE and Bluetooth
// and connects over Bluetooth, eventually upgrading to a Wifi Hotspot.
template <typename Platform>
class P2PStarPCPHandler : public P2PClusterPCPHandler<Platform> {
 public:
  P2PStarPCPHandler(Ptr<MediumManager<Platform> > medium_manager,
                    Ptr<EndpointManager<Platform> > endpoint_manager,
                    Ptr<EndpointChannelManager> endpoint_channel_manager,
                    Ptr<BandwidthUpgradeManager> bandwidth_upgrade_manager);
  ~P2PStarPCPHandler() override;

  Strategy getStrategy() override;
  PCP::Value getPCP() override;

 protected:
  std::vector<proto::connections::Medium> getConnectionMediumsByPriority()
      override;
  proto::connections::Medium getDefaultUpgradeMedium() override;

  bool canSendOutgoingConnection(
      Ptr<ClientProxy<Platform> > client_proxy) override;
  bool canReceiveIncomingConnection(
      Ptr<ClientProxy<Platform> > client_proxy) override;

 private:
  Ptr<MediumManager<Platform> > medium_manager_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/p2p_star_pcp_handler.cc"

#endif  // CORE_INTERNAL_P2P_STAR_PCP_HANDLER_H_
