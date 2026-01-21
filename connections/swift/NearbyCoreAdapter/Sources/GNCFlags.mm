// Copyright 2024 Google LLC
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

#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCFlags.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "internal/flags/nearby_flags.h"

@implementation GNCFlags

+ (BOOL)enableBLEV2 {
  return nearby::NearbyFlags::GetInstance().GetBoolFlag(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableBleV2);
}

+ (void)setEnableBLEV2:(BOOL)value {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableBleV2, value);
}

@end
