#ifndef CORE_INTERNAL_PCP_H_
#define CORE_INTERNAL_PCP_H_

namespace location {
namespace nearby {
namespace connections {

struct PCP {
  enum Value {
    UNKNOWN = 0,
    P2P_STAR = 1,
    P2P_CLUSTER = 2,
    P2P_POINT_TO_POINT = 3,
  };
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_PCP_H_
