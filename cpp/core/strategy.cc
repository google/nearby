#include "core/strategy.h"

namespace location {
namespace nearby {
namespace connections {

const Strategy Strategy::kP2PCluster(Strategy::ConnectionType::P2P,
                                     Strategy::TopologyType::M_TO_N);

const Strategy Strategy::kP2PStar(Strategy::ConnectionType::P2P,
                                  Strategy::TopologyType::ONE_TO_N);

const Strategy Strategy::kP2PPointToPoint(Strategy::ConnectionType::P2P,
                                          Strategy::TopologyType::ONE_TO_ONE);

Strategy::Strategy(ConnectionType::Value connection_type,
                   TopologyType::Value topology_type)
    : connection_type(connection_type), topology_type(topology_type) {}

Strategy::Strategy(const Strategy& that)
    : connection_type(that.connection_type),
      topology_type(that.topology_type) {}

bool Strategy::isValid() const {
  return kP2PStar == *this || kP2PCluster == *this || kP2PPointToPoint == *this;
}

std::string Strategy::getName() const {
  if (Strategy::kP2PCluster == *this) {
    return "P2P_CLUSTER";
  } else if (Strategy::kP2PStar == *this) {
    return "P2P_STAR";
  } else if (Strategy::kP2PPointToPoint == *this) {
    return "P2P_POINT_TO_POINT";
  } else {
    return "UNKNOWN";
  }
}

bool operator==(const Strategy& lhs, const Strategy& rhs) {
  return lhs.connection_type == rhs.connection_type &&
         lhs.topology_type == rhs.topology_type;
}

bool operator!=(const Strategy& lhs, const Strategy& rhs) {
  return !(lhs == rhs);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
