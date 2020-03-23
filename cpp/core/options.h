#ifndef CORE_OPTIONS_H_
#define CORE_OPTIONS_H_

#include "core/strategy.h"

namespace location {
namespace nearby {
namespace connections {

struct AdvertisingOptions {
  const Strategy strategy;
  const bool auto_upgrade_bandwidth;
  const bool enforce_topology_constraints;

  AdvertisingOptions(Strategy strategy, bool auto_upgrade_bandwidth,
                     bool enforce_topology_constraints)
      : strategy(strategy),
        auto_upgrade_bandwidth(auto_upgrade_bandwidth),
        enforce_topology_constraints(enforce_topology_constraints) {}
};

struct DiscoveryOptions {
  const Strategy strategy;

  explicit DiscoveryOptions(Strategy strategy) : strategy(strategy) {}
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_OPTIONS_H_
