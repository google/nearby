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

#ifndef CORE_INTERNAL_P2P_POINT_TO_POINT_PCP_HANDLER_H_
#define CORE_INTERNAL_P2P_POINT_TO_POINT_PCP_HANDLER_H_

#include "core/internal/bandwidth_upgrade_manager.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/medium_manager.h"
#include "core/internal/p2p_star_pcp_handler.h"
#include "core/internal/pcp.h"
#include "core/strategy.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

// Concrete implementation of the PCPHandler for the P2P_POINT_TO_POINT. This
// PCP is for mediums that have limitations on the number of simultaneous
// connections; all mediums in P2P_STAR are valid for P2P_POINT_TO_POINT, but
// not all mediums in P2P_POINT_TO_POINT and valid for P2P_STAR.
//
// <p>Currently, this implementation advertises/discovers over BLE and Bluetooth
// and connects over Bluetooth, eventually upgrading to Wifi Hotspot.
template <typename Platform>
class P2PPointToPointPCPHandler : public P2PStarPCPHandler<Platform> {
 public:
  P2PPointToPointPCPHandler(
      Ptr<MediumManager<Platform> > medium_manager,
      Ptr<EndpointManager<Platform> > endpoint_manager,
      Ptr<EndpointChannelManager> endpoint_channel_manager,
      Ptr<BandwidthUpgradeManager> bandwidth_upgrade_manager);

  Strategy getStrategy() override;
  PCP::Value getPCP() override;

 protected:
  std::vector<proto::connections::Medium> getConnectionMediumsByPriority()
      override;

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

#include "core/internal/p2p_point_to_point_pcp_handler.cc"

#endif  // CORE_INTERNAL_P2P_POINT_TO_POINT_PCP_HANDLER_H_
