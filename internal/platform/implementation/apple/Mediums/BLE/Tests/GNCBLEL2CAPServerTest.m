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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPServer.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheralManager.h"

@interface GNCBLEL2CAPServerTest : XCTestCase
@end

@implementation GNCBLEL2CAPServerTest

#pragma mark Tests

- (void)testPublishL2CAPChannelAndOpenChannelWhenStartListeningChannel {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager
                                                     queue:dispatch_get_main_queue()];
  XCTestExpectation *channelOpenedexpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Channel opened."];
  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  [l2capServer
      startListeningChannelWithPSMPublishedCompletionHandler:^(uint16_t PSM,
                                                               NSError *_Nullable error) {
        XCTAssertEqual(error, nil);
        XCTAssertEqual(PSM, fakePeripheralManager.PSM);
      }
      channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                       NSError *_Nullable error) {
        [channelOpenedexpectation fulfill];
      }];
  [self waitForExpectations:@[ channelOpenedexpectation ] timeout:0.5];
}

- (void)testFailedToPublishL2CAPChannel {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  fakePeripheralManager.didPublishL2CAPChannelError = [NSError errorWithDomain:@"fake"
                                                                          code:0
                                                                      userInfo:nil];
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager
                                                     queue:dispatch_get_main_queue()];
  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  [l2capServer
      startListeningChannelWithPSMPublishedCompletionHandler:^(uint16_t PSM,
                                                               NSError *_Nullable error) {
        XCTAssertEqual(error, fakePeripheralManager.didPublishL2CAPChannelError);
        XCTAssertEqual(PSM, 0);
      }
                              channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                                               NSError *_Nullable error){
                              }];
}

- (void)testPoweredOffUnpublishesChannel {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager
                                                     queue:dispatch_get_main_queue()];
  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  [l2capServer
      startListeningChannelWithPSMPublishedCompletionHandler:^(uint16_t PSM,
                                                               NSError *_Nullable error) {
        XCTAssertEqual(error, nil);
        XCTAssertEqual(PSM, fakePeripheralManager.PSM);
      }
                              channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                                               NSError *_Nullable error){
                              }];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOff];

  [self waitForExpectations:@[ fakePeripheralManager.unpublishExpectation ] timeout:0.0];
}

- (void)testStartPeripheralManagerInitiallyOff {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager
                                                     queue:dispatch_get_main_queue()];

  [l2capServer
      startListeningChannelWithPSMPublishedCompletionHandler:^(uint16_t PSM,
                                                               NSError *_Nullable error) {
        XCTAssertEqual(error, nil);
        XCTAssertEqual(PSM, fakePeripheralManager.PSM);
      }
                              channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                                               NSError *_Nullable error){
                              }];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];
}

- (void)testClose {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager
                                                     queue:dispatch_get_main_queue()];
  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  [l2capServer
      startListeningChannelWithPSMPublishedCompletionHandler:^(uint16_t PSM,
                                                               NSError *_Nullable error) {
        XCTAssertEqual(error, nil);
        XCTAssertEqual(PSM, fakePeripheralManager.PSM);
      }
                              channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                                               NSError *_Nullable error){
                              }];

  [l2capServer close];

  XCTAssertEqual([l2capServer PSM], 0);
}

- (void)testCloseDoesNotUnpublishesChannelIfNotConnected {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  fakePeripheralManager.unpublishExpectation.inverted = YES;
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager
                                                     queue:dispatch_get_main_queue()];

  [l2capServer close];

  [self waitForExpectations:@[ fakePeripheralManager.unpublishExpectation ] timeout:1.0];
}

@end
