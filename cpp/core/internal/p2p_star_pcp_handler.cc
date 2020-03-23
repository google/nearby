#include "core/internal/p2p_star_pcp_handler.h"

#include <vector>

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
P2PStarPCPHandler<Platform>::P2PStarPCPHandler(
    Ptr<MediumManager<Platform> > medium_manager,
    Ptr<EndpointManager<Platform> > endpoint_manager,
    Ptr<EndpointChannelManager<Platform> > endpoint_channel_manager,
    Ptr<BandwidthUpgradeManager<Platform> > bandwidth_upgrade_manager)
    : P2PClusterPCPHandler<Platform>(medium_manager, endpoint_manager,
                                     endpoint_channel_manager,
                                     bandwidth_upgrade_manager),
      medium_manager_(medium_manager) {}

template <typename Platform>
P2PStarPCPHandler<Platform>::~P2PStarPCPHandler() {}

template <typename Platform>
Strategy P2PStarPCPHandler<Platform>::getStrategy() {
  return Strategy::kP2PStar;
}

template <typename Platform>
PCP::Value P2PStarPCPHandler<Platform>::getPCP() {
  return PCP::P2P_STAR;
}

template <typename Platform>
std::vector<proto::connections::Medium>
P2PStarPCPHandler<Platform>::getConnectionMediumsByPriority() {
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
proto::connections::Medium
P2PStarPCPHandler<Platform>::getDefaultUpgradeMedium() {
  return proto::connections::Medium::WIFI_HOTSPOT;
}

template <typename Platform>
bool P2PStarPCPHandler<Platform>::canSendOutgoingConnection(
    Ptr<ClientProxy<Platform> > client_proxy) {
  // For star, we can only send an outgoing connection while we have no other
  // connections.
  return !this->hasOutgoingConnections(client_proxy) &&
      !this->hasIncomingConnections(client_proxy);
}

template <typename Platform>
bool P2PStarPCPHandler<Platform>::canReceiveIncomingConnection(
    Ptr<ClientProxy<Platform> > client_proxy) {
  // For star, we can only receive an incoming connection if we've sent no
  // outgoing connections.
  return !this->hasOutgoingConnections(client_proxy);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
