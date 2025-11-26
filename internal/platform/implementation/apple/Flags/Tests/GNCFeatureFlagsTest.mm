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

#import <XCTest/XCTest.h>

#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "internal/flags/nearby_flags.h"
#import "internal/platform/implementation/apple/Flags/GNCFeatureFlags.h"

@interface GNCFeatureFlagsTest : XCTestCase
@end

@implementation GNCFeatureFlagsTest

- (void)tearDown {
  // Reset flags to a known state (e.g., false) after each test.
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableDct, false);
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableBleL2cap,
      false);

  [super tearDown];
}

- (void)testDctEnabled_WhenFlagIsTrue {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableDct, YES);
  XCTAssertTrue([GNCFeatureFlags dctEnabled]);
}

- (void)testDctEnabled_WhenFlagIsFalse {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableDct, NO);
  XCTAssertFalse([GNCFeatureFlags dctEnabled]);
}

- (void)testGattClientDisconnectionEnabled_WhenFlagIsFalse {
  XCTAssertFalse([GNCFeatureFlags gattClientDisconnectionEnabled]);
}

- (void)testBleL2capEnabled_WhenFlagIsTrue {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableBleL2cap, YES);
  XCTAssertTrue([GNCFeatureFlags bleL2capEnabled]);
}

- (void)testBleL2capEnabled_WhenFlagIsFalse {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableBleL2cap, NO);
  XCTAssertFalse([GNCFeatureFlags bleL2capEnabled]);
}

@end
