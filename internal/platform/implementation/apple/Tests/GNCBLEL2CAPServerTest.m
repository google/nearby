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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPServer.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Tests/GNCBLEL2CAPServer+Testing.h"
#import "internal/platform/implementation/apple/Tests/GNCFakePeripheralManager.h"

static const int kPSM = 192;

@interface GNCBLEL2CAPServerTest : XCTestCase
@end

@implementation GNCBLEL2CAPServerTest

#pragma mark - Publish L2CAP channel

- (void)testPublishL2CAPChannel {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  fakePeripheralManager.PSM = kPSM;
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTAssertEqual([l2capServer PSM], kPSM);
  XCTAssertNotNil([l2capServer getChannel]);
}

- (void)testFailedToPublishL2CAPChannel {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  fakePeripheralManager.didPublishL2CAPChannelError = [NSError errorWithDomain:@"fake"
                                                                          code:0
                                                                      userInfo:nil];
  fakePeripheralManager.PSM = kPSM;
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTAssertNotEqual([l2capServer PSM], kPSM);
  XCTAssertNil([l2capServer getChannel]);
}

- (void)testUnPublishL2CAPChannel {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  fakePeripheralManager.PSM = kPSM;
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];
  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOff];

  XCTAssertEqual([l2capServer PSM], 0);
}

@end
