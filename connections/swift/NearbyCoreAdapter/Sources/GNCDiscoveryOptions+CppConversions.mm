// Copyright 2022 Google LLC
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

#import "connections/swift/NearbyCoreAdapter/Sources/GNCDiscoveryOptions+CppConversions.h"

#include "connections/discovery_options.h"

#import "connections/swift/NearbyCoreAdapter/Sources/GNCStrategy+Internal.h"
#import "connections/swift/NearbyCoreAdapter/Sources/GNCSupportedMediums+CppConversions.h"

using ::nearby::connections::CppStrategyFromGNCStrategy;
using ::nearby::connections::DiscoveryOptions;

@implementation GNCDiscoveryOptions (CppConversions)

- (DiscoveryOptions)toCpp {
  DiscoveryOptions discovery_options;

  discovery_options.strategy = CppStrategyFromGNCStrategy(self.strategy);
  discovery_options.allowed = [self.mediums toCpp];

  discovery_options.enforce_topology_constraints = self.enforceTopologyConstraints;
  discovery_options.low_power = self.lowPower;

  return discovery_options;
}

@end
