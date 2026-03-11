// Copyright 2025 Google LLC
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

#import "internal/platform/implementation/apple/Flags/GNCFeatureFlags.h"

#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "internal/flags/nearby_flags.h"

@implementation GNCFeatureFlags

+ (BOOL)dctEnabled {
  return nearby::NearbyFlags::GetInstance().GetBoolFlag(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableDct);
}

+ (BOOL)gattClientDisconnectionEnabled {
  return nearby::NearbyFlags::GetInstance().GetBoolFlag(
      nearby::connections::config_package_nearby::nearby_connections_feature::
          kEnableGattClientDisconnection);
}

+ (BOOL)bleL2capEnabled {
  return nearby::NearbyFlags::GetInstance().GetBoolFlag(
      nearby::connections::config_package_nearby::nearby_connections_feature::
          kEnableBleL2cap);
}

+ (BOOL)refactorBleL2capEnabled {
  return nearby::NearbyFlags::GetInstance().GetBoolFlag(
      nearby::connections::config_package_nearby::nearby_connections_feature::
          kRefactorBleL2cap);
}

+ (BOOL)sharedPeripheralManagerEnabled {
  return nearby::NearbyFlags::GetInstance().GetBoolFlag(
      nearby::connections::config_package_nearby::nearby_connections_feature::
          kEnableSharedPeripheralManager);
}

@end
