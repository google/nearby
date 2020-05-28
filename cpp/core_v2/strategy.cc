#include "core_v2/strategy.h"

namespace location {
namespace nearby {
namespace connections {

const Strategy Strategy::kNone = {Strategy::ConnectionType::kNone,
                                  Strategy::TopologyType::kUnknown};
const Strategy Strategy::kP2pCluster{Strategy::ConnectionType::kPointToPoint,
                                     Strategy::TopologyType::kManyToMany};
const Strategy Strategy::kP2pStar{Strategy::ConnectionType::kPointToPoint,
                                  Strategy::TopologyType::kOneToMany};
const Strategy Strategy::kP2pPointToPoint{
    Strategy::ConnectionType::kPointToPoint, Strategy::TopologyType::kOneToOne};

bool Strategy::IsNone() const {
  return *this == kNone;
}

bool Strategy::IsValid() const {
  return *this == kP2pStar || *this == kP2pCluster || *this ==kP2pPointToPoint;
}

std::string Strategy::GetName() const {
  if (*this == Strategy::kP2pCluster) {
    return "P2P_CLUSTER";
  } else if (*this == Strategy::kP2pStar) {
    return "P2P_STAR";
  } else if (*this == Strategy::kP2pPointToPoint) {
    return "P2P_POINT_TO_POINT";
  } else {
    return "UNKNOWN";
  }
}

bool operator==(const Strategy& lhs, const Strategy& rhs) {
  return lhs.connection_type_ == rhs.connection_type_ &&
         lhs.topology_type_ == rhs.topology_type_;
}

bool operator!=(const Strategy& lhs, const Strategy& rhs) {
  return !(lhs == rhs);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
