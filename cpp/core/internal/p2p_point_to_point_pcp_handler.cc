#include "core/internal/p2p_point_to_point_pcp_handler.h"

namespace location {
namespace nearby {
namespace connections {

P2pPointToPointPcpHandler::P2pPointToPointPcpHandler(
    Mediums& mediums, EndpointManager& endpoint_manager,
    EndpointChannelManager& channel_manager, BwuManager& bwu_manager,
    InjectedBluetoothDeviceStore& injected_bluetooth_device_store, Pcp pcp)
    : P2pStarPcpHandler(mediums, endpoint_manager, channel_manager, bwu_manager,
                        injected_bluetooth_device_store, pcp) {}

std::vector<proto::connections::Medium>
P2pPointToPointPcpHandler::GetConnectionMediumsByPriority() {
  std::vector<proto::connections::Medium> mediums;
  if (mediums_->GetWifiLan().IsAvailable()) {
    mediums.push_back(proto::connections::WIFI_LAN);
  }
  if (mediums_->GetWebRtc().IsAvailable()) {
    mediums.push_back(proto::connections::WEB_RTC);
  }
  if (mediums_->GetBluetoothClassic().IsAvailable()) {
    mediums.push_back(proto::connections::BLUETOOTH);
  }
  if (mediums_->GetBle().IsAvailable()) {
    mediums.push_back(proto::connections::BLE);
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
