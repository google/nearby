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

#import "connections/swift/NearbyCoreAdapter/Sources/GNCConnectionOptions+CppConversions.h"

#include "connections/connection_options.h"
#include "internal/platform/byte_array.h"

#import "connections/swift/NearbyCoreAdapter/Sources/GNCSupportedMediums+CppConversions.h"
#import "GoogleToolboxForMac/GTMLogger.h"

using ::nearby::connections::ConnectionOptions;

@implementation GNCConnectionOptions (CppConversions)

- (ConnectionOptions)toCpp {
  ConnectionOptions connection_options;

  connection_options.allowed = [self.mediums toCpp];

  connection_options.auto_upgrade_bandwidth = self.autoUpgradeBandwidth;
  connection_options.low_power = self.lowPower;
  connection_options.enforce_topology_constraints = self.enforceTopologyConstraints;
  if (self.enforceTopologyConstraints) {
    GTMLoggerError(@"WARNING: Creating ConnectionOptions with enforceTopologyConstraints = true. "
                    "Make sure you know what you're doing!");
  }

  connection_options.keep_alive_interval_millis = self.keepAliveIntervalInSeconds * 1000;
  connection_options.keep_alive_timeout_millis = self.keepAliveTimeoutInSeconds * 1000;

  connection_options.remote_bluetooth_mac_address = nearby::ByteArray(
      (char *)[self.remoteBluetoothMACAddress bytes], [self.remoteBluetoothMACAddress length]);

  return connection_options;
}

@end
