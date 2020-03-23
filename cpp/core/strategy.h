#ifndef CORE_STRATEGY_H_
#define CORE_STRATEGY_H_

#include "platform/port/string.h"

namespace location {
namespace nearby {
namespace connections {

struct Strategy {
 public:
  static const Strategy kP2PCluster;
  static const Strategy kP2PStar;
  static const Strategy kP2PPointToPoint;

  Strategy(const Strategy& that);

  bool isValid() const;
  std::string getName() const;

  friend bool operator==(const Strategy& lhs, const Strategy& rhs);
  friend bool operator!=(const Strategy& lhs, const Strategy& rhs);

 private:
  struct ConnectionType {
    enum Value { P2P = 1 };
  };
  struct TopologyType {
    enum Value { ONE_TO_ONE = 1, ONE_TO_N = 2, M_TO_N = 3 };
  };

  const ConnectionType::Value connection_type;
  const TopologyType::Value topology_type;

  Strategy(ConnectionType::Value connection_type,
           TopologyType::Value topology_type);
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_STRATEGY_H_
