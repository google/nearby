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

#ifndef CORE_V2_INTERNAL_P2P_STAR_PCP_HANDLER_H_
#define CORE_V2_INTERNAL_P2P_STAR_PCP_HANDLER_H_

#include <vector>

#include "core_v2/internal/client_proxy.h"
#include "core_v2/internal/endpoint_channel_manager.h"
#include "core_v2/internal/endpoint_manager.h"
#include "core_v2/internal/p2p_cluster_pcp_handler.h"
#include "core_v2/internal/pcp.h"
#include "core_v2/strategy.h"

namespace location {
namespace nearby {
namespace connections {

// Concrete implementation of the PcpHandler for the P2P_STAR PCP. This Pcp is
// for mediums that have one server with (potentially) many clients; all mediums
// in P2P_CLUSTER are valid for P2P_STAR, but not all mediums in P2P_STAR are
// valid for P2P_CLUSTER.
//
// Currently, this implementation advertises/discovers over Bluetooth
// and connects over Bluetooth.
class P2pStarPcpHandler : public P2pClusterPcpHandler {
 public:
  P2pStarPcpHandler(Mediums& mediums, EndpointManager& endpoint_manager,
                    EndpointChannelManager& channel_manager,
                    BwuManager& bwu_manager,
                    Pcp pcp = Pcp::kP2pStar);

 protected:
  std::vector<proto::connections::Medium> GetConnectionMediumsByPriority()
      override;
  proto::connections::Medium GetDefaultUpgradeMedium() override;

  bool CanSendOutgoingConnection(ClientProxy* client) const override;
  bool CanReceiveIncomingConnection(ClientProxy* client) const override;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_P2P_STAR_PCP_HANDLER_H_
