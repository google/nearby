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

#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPServer+Testing.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheralManager.h"

static const NSTimeInterval kTestTimeout = 1.0;

@interface GNCBLEL2CAPServerTest : XCTestCase
@end

@implementation GNCBLEL2CAPServerTest

#pragma mark Tests

- (void)testInit {
  GNCBLEL2CAPServer *l2capServer = [[GNCBLEL2CAPServer alloc] init];
  XCTAssertNotNil(l2capServer);
  XCTAssertNil([l2capServer valueForKey:@"_peripheralManager"]);
}

- (void)testPublishL2CAPChannelAndOpenChannelWhenStartListeningChannel {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager
                                                     queue:dispatch_get_main_queue()];
  XCTestExpectation *psmPublishedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"PSM published."];
  XCTestExpectation *channelOpenedexpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Channel opened."];
  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  [l2capServer
      startListeningChannelWithPSMPublishedCompletionHandler:^(uint16_t PSM,
                                                               NSError *_Nullable error) {
        XCTAssertEqual(error, nil);
        XCTAssertEqual(PSM, fakePeripheralManager.PSM);
        [psmPublishedExpectation fulfill];
      }
      channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                       NSError *_Nullable error) {
        XCTAssertNil(error);
        XCTAssertNotNil(stream);
        [channelOpenedexpectation fulfill];
      }];
  [self waitForExpectations:@[ psmPublishedExpectation, channelOpenedexpectation ]
                    timeout:kTestTimeout];
  XCTAssertNotNil([l2capServer valueForKey:@"l2CAPChannel"]);
  XCTAssertNotNil([l2capServer valueForKey:@"l2CAPStream"]);
}

- (void)testFailedToPublishL2CAPChannel {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  fakePeripheralManager.didPublishL2CAPChannelError = [NSError errorWithDomain:@"fake"
                                                                          code:0
                                                                      userInfo:nil];
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager
                                                     queue:dispatch_get_main_queue()];
  XCTestExpectation *psmPublishedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"PSM published with error."];
  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  [l2capServer
      startListeningChannelWithPSMPublishedCompletionHandler:^(uint16_t PSM,
                                                               NSError *_Nullable error) {
        XCTAssertEqual(error, fakePeripheralManager.didPublishL2CAPChannelError);
        XCTAssertEqual(PSM, 0);
        [psmPublishedExpectation fulfill];
      }
                              channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                                               NSError *_Nullable error){
                              }];
  [self waitForExpectations:@[ psmPublishedExpectation ] timeout:kTestTimeout];
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

  [self waitForExpectations:@[ fakePeripheralManager.unpublishExpectation ] timeout:kTestTimeout];
}

- (void)testPeripheralManagerDidUpdateStatePoweredOffUnpublishesChannel {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager
                                                     queue:dispatch_get_main_queue()];
  fakePeripheralManager.state = CBManagerStatePoweredOn;
  [(id<CBPeripheralManagerDelegate>)l2capServer
      peripheralManagerDidUpdateState:(CBPeripheralManager *)fakePeripheralManager];

  [l2capServer
      startListeningChannelWithPSMPublishedCompletionHandler:^(uint16_t PSM,
                                                               NSError *_Nullable error) {
        XCTAssertEqual(error, nil);
        XCTAssertEqual(PSM, fakePeripheralManager.PSM);
      }
                              channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                                               NSError *_Nullable error){
                              }];

  fakePeripheralManager.state = CBManagerStatePoweredOff;
  [(id<CBPeripheralManagerDelegate>)l2capServer
      peripheralManagerDidUpdateState:(CBPeripheralManager *)fakePeripheralManager];

  [self waitForExpectations:@[ fakePeripheralManager.unpublishExpectation ] timeout:kTestTimeout];
}

- (void)testStartPeripheralManagerInitiallyOff {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager
                                                     queue:dispatch_get_main_queue()];
  XCTestExpectation *psmPublishedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"PSM published."];
  XCTestExpectation *channelOpenedexpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Channel opened."];

  [l2capServer
      startListeningChannelWithPSMPublishedCompletionHandler:^(uint16_t PSM,
                                                               NSError *_Nullable error) {
        XCTAssertEqual(error, nil);
        XCTAssertEqual(PSM, fakePeripheralManager.PSM);
        [psmPublishedExpectation fulfill];
      }
      channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                       NSError *_Nullable error) {
        [channelOpenedexpectation fulfill];
      }];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];
  [self waitForExpectations:@[ psmPublishedExpectation, channelOpenedexpectation ]
                    timeout:kTestTimeout];
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

  [self waitForExpectations:@[ fakePeripheralManager.unpublishExpectation ] timeout:kTestTimeout];
}

- (void)testFailedToOpenL2CAPChannel {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  fakePeripheralManager.didOpenL2CAPChannelError = [NSError errorWithDomain:@"fake"
                                                                       code:0
                                                                   userInfo:nil];
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
        XCTAssertNil(stream);
        XCTAssertEqual(error, fakePeripheralManager.didOpenL2CAPChannelError);
        [channelOpenedexpectation fulfill];
      }];
  [self waitForExpectations:@[ channelOpenedexpectation ] timeout:kTestTimeout];
  XCTAssertNil([l2capServer valueForKey:@"l2CAPChannel"]);
  XCTAssertNil([l2capServer valueForKey:@"l2CAPStream"]);
}

- (void)testPeripheralManagerDidUnpublishL2CAPChannel {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager
                                                     queue:dispatch_get_main_queue()];
  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];
  [l2capServer
      startListeningChannelWithPSMPublishedCompletionHandler:^(uint16_t PSM,
                                                               NSError *_Nullable error) {
      }
                              channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                                               NSError *_Nullable error){
                              }];

  [(id<CBPeripheralManagerDelegate>)l2capServer
             peripheralManager:(CBPeripheralManager *)fakePeripheralManager
      didUnpublishL2CAPChannel:l2capServer.PSM
                         error:[NSError errorWithDomain:@"fake" code:0 userInfo:nil]];

  XCTAssertEqual(l2capServer.PSM, 0);
}

- (void)testCloseL2CAPChannel {
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
      }
      channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                       NSError *_Nullable error) {
        [channelOpenedexpectation fulfill];
      }];
  [self waitForExpectations:@[ channelOpenedexpectation ] timeout:kTestTimeout];

  [l2capServer closeL2CAPChannel];

  XCTAssertNil([l2capServer valueForKey:@"l2CAPChannel"]);
  XCTAssertNil([l2capServer valueForKey:@"l2CAPStream"]);
}

- (void)testPeripheralManagerDidPublishL2CAPChannel {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager
                                                     queue:dispatch_get_main_queue()];
  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"completion called"];
  [l2capServer
      startListeningChannelWithPSMPublishedCompletionHandler:^(uint16_t PSM,
                                                               NSError *_Nullable error) {
        XCTAssertEqual(PSM, 1);
        XCTAssertNil(error);
        [expectation fulfill];
      }
                              channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                                               NSError *_Nullable error){
                              }];
  [(id<CBPeripheralManagerDelegate>)l2capServer
           peripheralManager:(CBPeripheralManager *)fakePeripheralManager
      didPublishL2CAPChannel:1
                       error:nil];
  [self waitForExpectations:@[ expectation ] timeout:kTestTimeout];
  XCTAssertEqual(l2capServer.PSM, 1);
}

- (void)testPeripheralManagerDidPublishL2CAPChannelWithError {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEL2CAPServer *l2capServer =
      [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:fakePeripheralManager
                                                     queue:dispatch_get_main_queue()];
  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"completion called"];
  NSError *publishError = [NSError errorWithDomain:@"test" code:0 userInfo:nil];
  [l2capServer
      startListeningChannelWithPSMPublishedCompletionHandler:^(uint16_t PSM,
                                                               NSError *_Nullable error) {
        XCTAssertEqual(PSM, 0);
        XCTAssertEqualObjects(error, publishError);
        [expectation fulfill];
      }
                              channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                                               NSError *_Nullable error){
                              }];
  [(id<CBPeripheralManagerDelegate>)l2capServer
           peripheralManager:(CBPeripheralManager *)fakePeripheralManager
      didPublishL2CAPChannel:0
                       error:publishError];
  [self waitForExpectations:@[ expectation ] timeout:kTestTimeout];
  XCTAssertEqual(l2capServer.PSM, 0);
}

@end
