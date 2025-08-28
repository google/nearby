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

#include "absl/types/span.h"
#include "connections/connection_options.h"
#include "internal/platform/mac_address.h"

#import "connections/swift/NearbyCoreAdapter/Sources/GNCSupportedMediums+CppConversions.h"
#import "internal/platform/implementation/apple/Log/GNCLogger.h"

using ::nearby::connections::ConnectionOptions;

@implementation GNCConnectionOptions (CppConversions)

- (ConnectionOptions)toCpp {
  ConnectionOptions connection_options;

  connection_options.allowed = [self.mediums toCpp];

  connection_options.auto_upgrade_bandwidth = self.autoUpgradeBandwidth;
  connection_options.low_power = self.lowPower;
  connection_options.enforce_topology_constraints = self.enforceTopologyConstraints;
  if (self.enforceTopologyConstraints) {
    GNCLoggerError(@"WARNING: Creating ConnectionOptions with enforceTopologyConstraints = true. "
                    "Make sure you know what you're doing!");
  }

  connection_options.keep_alive_interval_millis = self.keepAliveIntervalInSeconds * 1000;
  connection_options.keep_alive_timeout_millis = self.keepAliveTimeoutInSeconds * 1000;

  nearby::MacAddress mac_address;
  nearby::MacAddress::FromBytes(
      absl::MakeConstSpan(
          (const uint8_t*)[self.remoteBluetoothMACAddress bytes],
          [self.remoteBluetoothMACAddress length]),
      mac_address);
  connection_options.remote_bluetooth_mac_address = mac_address;

  return connection_options;
}

@end
