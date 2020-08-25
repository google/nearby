#include "core_v2/internal/p2p_star_pcp_handler.h"

#include <vector>

namespace location {
namespace nearby {
namespace connections {

P2pStarPcpHandler::P2pStarPcpHandler(Mediums& mediums,
                                     EndpointManager& endpoint_manager,
                                     EndpointChannelManager& channel_manager,
                                     Pcp pcp)
    : P2pClusterPcpHandler(&mediums, &endpoint_manager, &channel_manager, pcp) {
}

std::vector<proto::connections::Medium>
P2pStarPcpHandler::GetConnectionMediumsByPriority() {
  std::vector<proto::connections::Medium> mediums;
  if (mediums_->GetBluetoothClassic().IsAvailable()) {
    mediums.push_back(proto::connections::BLUETOOTH);
  }
  return mediums;
}

proto::connections::Medium P2pStarPcpHandler::GetDefaultUpgradeMedium() {
  return proto::connections::Medium::WIFI_HOTSPOT;
}

bool P2pStarPcpHandler::CanSendOutgoingConnection(ClientProxy* client) const {
  // For star, we can only send an outgoing connection while we have no other
  // connections.
  return !this->HasOutgoingConnections(client) &&
         !this->HasIncomingConnections(client);
}

bool P2pStarPcpHandler::CanReceiveIncomingConnection(
    ClientProxy* client) const {
  // For star, we can only receive an incoming connection if we've sent no
  // outgoing connections.
  return !this->HasOutgoingConnections(client);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
