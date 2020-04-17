// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
