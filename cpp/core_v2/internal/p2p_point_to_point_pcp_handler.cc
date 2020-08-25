#include "core_v2/internal/p2p_point_to_point_pcp_handler.h"

namespace location {
namespace nearby {
namespace connections {

P2pPointToPointPcpHandler::P2pPointToPointPcpHandler(
    Mediums& mediums, EndpointManager& endpoint_manager,
    EndpointChannelManager& channel_manager, Pcp pcp)
    : P2pStarPcpHandler(mediums, endpoint_manager, channel_manager, pcp) {}

std::vector<proto::connections::Medium>
P2pPointToPointPcpHandler::GetConnectionMediumsByPriority() {
  std::vector<proto::connections::Medium> mediums;
  if (mediums_->GetBluetoothClassic().IsAvailable()) {
    mediums.push_back(proto::connections::BLUETOOTH);
  }
  return mediums;
}

bool P2pPointToPointPcpHandler::CanSendOutgoingConnection(
    ClientProxy* client) const {
  // For point to point, we can only send an outgoing connection while we have
  // no other connections.
  return !this->HasOutgoingConnections(client) &&
         !this->HasIncomingConnections(client);
}

bool P2pPointToPointPcpHandler::CanReceiveIncomingConnection(
    ClientProxy* client) const {
  // For point to point, we can only receive an incoming connection while we
  // have no other connections.
  return !this->HasOutgoingConnections(client) &&
         !this->HasIncomingConnections(client);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
