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

#include "core/internal/p2p_point_to_point_pcp_handler.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
P2PPointToPointPCPHandler<Platform>::P2PPointToPointPCPHandler(
    Ptr<MediumManager<Platform> > medium_manager,
    Ptr<EndpointManager<Platform> > endpoint_manager,
    Ptr<EndpointChannelManager> endpoint_channel_manager,
    Ptr<BandwidthUpgradeManager> bandwidth_upgrade_manager)
    : P2PStarPCPHandler<Platform>(medium_manager, endpoint_manager,
                                  endpoint_channel_manager,
                                  bandwidth_upgrade_manager),
      medium_manager_(medium_manager) {}

template <typename Platform>
Strategy P2PPointToPointPCPHandler<Platform>::getStrategy() {
  return Strategy::kP2PPointToPoint;
}

template <typename Platform>
PCP::Value P2PPointToPointPCPHandler<Platform>::getPCP() {
  return PCP::P2P_POINT_TO_POINT;
}

template <typename Platform>
std::vector<proto::connections::Medium>
P2PPointToPointPCPHandler<Platform>::getConnectionMediumsByPriority() {
  std::vector<proto::connections::Medium> mediums;
  if (medium_manager_->isBluetoothAvailable()) {
    mediums.push_back(proto::connections::BLUETOOTH);
  }
  if (medium_manager_->isBleAvailable()) {
    mediums.push_back(proto::connections::BLE);
  }
  return mediums;
}

template <typename Platform>
bool P2PPointToPointPCPHandler<Platform>::canSendOutgoingConnection(
    Ptr<ClientProxy<Platform> > client_proxy) {
  // For point to point, we can only send an outgoing connection while we have
  // no other connections.
  return !this->hasOutgoingConnections(client_proxy) &&
      !this->hasIncomingConnections(client_proxy);
}

template <typename Platform>
bool P2PPointToPointPCPHandler<Platform>::canReceiveIncomingConnection(
    Ptr<ClientProxy<Platform> > client_proxy) {
  // For point to point, we can only receive an incoming connection while we
  // have no other connections.
  return !this->hasOutgoingConnections(client_proxy) &&
      !this->hasIncomingConnections(client_proxy);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
