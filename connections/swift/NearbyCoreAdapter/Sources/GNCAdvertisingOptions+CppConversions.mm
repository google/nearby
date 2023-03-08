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

#import "connections/swift/NearbyCoreAdapter/Sources/GNCAdvertisingOptions+CppConversions.h"

#include "connections/advertising_options.h"

#import "connections/swift/NearbyCoreAdapter/Sources/GNCStrategy+Internal.h"
#import "connections/swift/NearbyCoreAdapter/Sources/GNCSupportedMediums+CppConversions.h"
#import "GoogleToolboxForMac/GTMLogger.h"

using ::nearby::connections::AdvertisingOptions;
using ::nearby::connections::CppStrategyFromGNCStrategy;

@implementation GNCAdvertisingOptions (CppConversions)

- (AdvertisingOptions)toCpp {
  AdvertisingOptions advertising_options;

  advertising_options.strategy = CppStrategyFromGNCStrategy(self.strategy);
  advertising_options.allowed = [self.mediums toCpp];

  advertising_options.auto_upgrade_bandwidth = self.autoUpgradeBandwidth;
  advertising_options.low_power = self.lowPower;
  advertising_options.enforce_topology_constraints = self.enforceTopologyConstraints;
  if (self.enforceTopologyConstraints) {
    GTMLoggerError(@"WARNING: Creating ConnectionOptions with enforceTopologyConstraints = true. "
                    "Make sure you know what you're doing!");
  }

  return advertising_options;
}

@end
