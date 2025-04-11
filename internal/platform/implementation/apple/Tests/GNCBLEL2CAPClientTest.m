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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPClient.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Tests/GNCFakePeripheral.h"

@interface GNCBLEL2CAPClientTest : XCTestCase
@end

@implementation GNCBLEL2CAPClientTest

#pragma mark - Tests

- (void)testDisconnect {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];
  XCTestExpectation *expectation = [[XCTestExpectation alloc] initWithDescription:@"Disconnect."];

  GNCBLEL2CAPClient *l2capClient =
      [[GNCBLEL2CAPClient alloc] initWithPeripheral:fakePeripheral
                        requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull peripheral) {
                          XCTAssertNotNil(peripheral);
                          [expectation fulfill];
                        }];

  [l2capClient disconnect];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

@end

