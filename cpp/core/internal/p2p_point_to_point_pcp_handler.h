#ifndef CORE_INTERNAL_P2P_POINT_TO_POINT_PCP_HANDLER_H_
#define CORE_INTERNAL_P2P_POINT_TO_POINT_PCP_HANDLER_H_

#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/p2p_star_pcp_handler.h"
#include "core/internal/pcp.h"
#include "core/strategy.h"

namespace location {
namespace nearby {
namespace connections {

// Concrete implementation of the PCPHandler for the P2P_POINT_TO_POINT. This
// PCP is for mediums that have limitations on the number of simultaneous
// connections; all mediums in P2P_STAR are valid for P2P_POINT_TO_POINT, but
// not all mediums in P2P_POINT_TO_POINT are valid for P2P_STAR.
//
// Currently, this implementation advertises/discovers over Bluetooth
// and connects over Bluetooth.
class P2pPointToPointPcpHandler : public P2pStarPcpHandler {
 public:
  P2pPointToPointPcpHandler(
      Mediums& mediums, EndpointManager& endpoint_manager,
      EndpointChannelManager& channel_manager, BwuManager& bwu_manager,
      InjectedBluetoothDeviceStore& injected_bluetooth_device_store,
      Pcp pcp = Pcp::kP2pPointToPoint);

 protected:
  std::vector<proto::connections::Medium> GetConnectionMediumsByPriority()
      override;

  bool CanSendOutgoingConnection(ClientProxy* client) const override;
  bool CanReceiveIncomingConnection(ClientProxy* client) const override;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_P2P_POINT_TO_POINT_PCP_HANDLER_H_
