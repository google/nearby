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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPClient.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPClient+Testing.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheral.h"

@interface GNCBLEL2CAPClientTest : XCTestCase
@property(nonatomic) GNCFakePeripheral *fakePeripheral;
@property(nonatomic) GNCBLEL2CAPClient *l2capClient;
@property(nonatomic) XCTestExpectation *requestDisconnectionHandlerExpectation;
@end

@implementation GNCBLEL2CAPClientTest

#pragma mark - Tests

- (void)setUp {
  [super setUp];
  self.fakePeripheral = [[GNCFakePeripheral alloc] init];
  self.l2capClient =
      [[GNCBLEL2CAPClient alloc] initWithQueue:nil
                   requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull peripheral) {
                     XCTAssertNotNil(peripheral);
                     [self.requestDisconnectionHandlerExpectation fulfill];
                   }];
}

- (void)tearDown {
  self.fakePeripheral = nil;
  self.l2capClient = nil;
  [super tearDown];
}

- (void)testSuccessfulOpenL2CAPChannel {
  XCTestExpectation *expectation =
      [self expectationWithDescription:@"L2CAP channel opened successfully"];
  uint16_t psm = 123;

  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(error, @"Error should be nil");
                            [expectation fulfill];
                          }];

  [self waitForExpectations:@[ expectation ] timeout:1.0];
}

- (void)testOpenL2CAPChannelWithError {
  XCTestExpectation *expectation =
      [self expectationWithDescription:@"L2CAP channel open failed with error"];
  uint16_t psm = 123;
  NSError *expectedError = [NSError errorWithDomain:@"TestErrorDomain" code:1 userInfo:nil];
  self.fakePeripheral.openL2CAPChannelError = expectedError;

  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNotNil(error, @"Error should not be nil");
                            XCTAssertEqualObjects(error, expectedError);
                            [expectation fulfill];
                          }];

  [self waitForExpectations:@[ expectation ] timeout:1.0];
}

- (void)testDisconnect {
  self.requestDisconnectionHandlerExpectation =
      [self expectationWithDescription:@"Request disconnection handler should be called"];
  uint16_t psm = 123;

  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error){
                          }];

  [self.l2capClient disconnect];

  [self waitForExpectations:@[ self.requestDisconnectionHandlerExpectation ] timeout:1.0];
}

@end
