#ifndef CORE_V2_OPTIONS_H_
#define CORE_V2_OPTIONS_H_

#include "core_v2/strategy.h"

namespace location {
namespace nearby {
namespace connections {

// Connection Options: used for both Advertising and Discovery.
// All fields are mutable, to make the type copy-assignable.
struct ConnectionOptions {
  Strategy strategy;
  bool auto_upgrade_bandwidth;
  bool enforce_topology_constraints;
  // Verify if  ConnectionOptions is in a not-initialized (Empty) state.
  bool Empty() const {
    return strategy.IsNone();
  }
  // Bring  ConnectionOptions to a not-initialized (Empty) state.
  void Clear() {
    strategy.Clear();
  }
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_OPTIONS_H_
