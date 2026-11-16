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
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPFakeInputOutputStream.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheral.h"

static const NSTimeInterval kTestTimeout = 1.0;

@interface GNCFakeGATTDelegate : NSObject <GNCPeripheralDelegate>
@property(nonatomic) BOOL didDiscoverServicesCalled;
@property(nonatomic) BOOL didDiscoverCharacteristicsCalled;
@property(nonatomic) BOOL didUpdateValueCalled;
@end

@implementation GNCFakeGATTDelegate
- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral didDiscoverServices:(nullable NSError *)error {
  _didDiscoverServicesCalled = YES;
}
- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(nullable NSError *)error {
  _didDiscoverCharacteristicsCalled = YES;
}
- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(nullable NSError *)error {
  _didUpdateValueCalled = YES;
}
@end

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
                            XCTAssertNil(stream, @"Stream should be nil due to testing limitation");
                            [expectation fulfill];
                          }];

  [self waitForExpectations:@[ expectation ] timeout:kTestTimeout];
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

  [self waitForExpectations:@[ expectation ] timeout:kTestTimeout];
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

  [self waitForExpectations:@[ self.requestDisconnectionHandlerExpectation ] timeout:kTestTimeout];
}

- (void)testOpenL2CAPChannelSetsPeripheralDelegate {
  uint16_t psm = 123;

  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error){
                          }];

  XCTAssertEqualObjects(self.fakePeripheral.peripheralDelegate, self.l2capClient);
}

- (void)testInitWithRequestDisconnectionHandlerAndDisconnect {
  XCTestExpectation *expectation =
      [self expectationWithDescription:@"Disconnection handler called"];
  GNCBLEL2CAPClient *client = [[GNCBLEL2CAPClient alloc]
      initWithRequestDisconnectionHandler:^(id<GNCPeripheral> _Nullable peripheral) {
        XCTAssertNil(peripheral);
        [expectation fulfill];
      }];
  [client disconnect];
  [self waitForExpectations:@[ expectation ] timeout:kTestTimeout];
}

- (void)testCloseL2CAPChannel {
  self.requestDisconnectionHandlerExpectation =
      [self expectationWithDescription:@"Request disconnection handler should be called"];
  uint16_t psm = 123;

  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error){
                          }];

  [self.l2capClient closeL2CAPChannel];

  [self waitForExpectations:@[ self.requestDisconnectionHandlerExpectation ] timeout:kTestTimeout];
}

- (void)testDidOpenL2CAPChannelWithStreams {
  XCTestExpectation *expectation =
      [self expectationWithDescription:@"L2CAP channel opened successfully"];
  uint16_t psm = 123;
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  self.fakePeripheral.channelInputStream = fakeStreams.inputStream;
  self.fakePeripheral.channelOutputStream = fakeStreams.outputStream;

  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(error);
                            XCTAssertNotNil(stream);
                            [expectation fulfill];
                          }];

  [self waitForExpectations:@[ expectation ] timeout:kTestTimeout];
}

- (void)testDidOpenL2CAPChannelCalledTwice {
  XCTestExpectation *expectation = [self expectationWithDescription:@"L2CAP channel opened twice"];
  expectation.expectedFulfillmentCount = 2;
  self.requestDisconnectionHandlerExpectation =
      [self expectationWithDescription:@"Request disconnection handler should be called"];
  uint16_t psm = 123;
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  self.fakePeripheral.channelInputStream = fakeStreams.inputStream;
  self.fakePeripheral.channelOutputStream = fakeStreams.outputStream;

  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(error);
                            XCTAssertNotNil(stream);
                            [expectation fulfill];
                          }];

  // Calling gnc_peripheral again to simulate channel opened again.
  CBL2CAPChannel *channel = [[CBL2CAPChannel alloc] init];
  [channel setValue:fakeStreams.inputStream forKey:@"inputStream"];
  [channel setValue:fakeStreams.outputStream forKey:@"outputStream"];
  [self.l2capClient peripheral:(CBPeripheral *)self.fakePeripheral
           didOpenL2CAPChannel:channel
                         error:nil];

  [self waitForExpectations:@[ expectation, self.requestDisconnectionHandlerExpectation ]
                    timeout:kTestTimeout];
}

- (void)testDelegateRestoredAndGATTCallbacksForwardedDuringOpen {
  GNCFakeGATTDelegate *gattDelegate = [[GNCFakeGATTDelegate alloc] init];
  self.fakePeripheral.peripheralDelegate = gattDelegate;
  self.fakePeripheral.delegateDelay = 0.05;

  XCTestExpectation *expectation =
      [self expectationWithDescription:@"L2CAP channel open completed"];
  [self.l2capClient openL2CAPChannelWithPSM:123
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(error);
                            [expectation fulfill];
                          }];

  XCTAssertEqual(self.fakePeripheral.peripheralDelegate, self.l2capClient);
  [self.fakePeripheral.peripheralDelegate gnc_peripheral:self.fakePeripheral
                                     didDiscoverServices:nil];
  [self.fakePeripheral.peripheralDelegate gnc_peripheral:self.fakePeripheral
                    didDiscoverCharacteristicsForService:(CBService *)[[NSObject alloc] init]
                                                   error:nil];
  [self.fakePeripheral.peripheralDelegate gnc_peripheral:self.fakePeripheral
                         didUpdateValueForCharacteristic:(CBCharacteristic *)[[NSObject alloc] init]
                                                   error:nil];
  XCTAssertTrue(gattDelegate.didDiscoverServicesCalled);
  XCTAssertTrue(gattDelegate.didDiscoverCharacteristicsCalled);
  XCTAssertTrue(gattDelegate.didUpdateValueCalled);

  [self waitForExpectations:@[ expectation ] timeout:kTestTimeout];
  XCTAssertEqual(self.fakePeripheral.peripheralDelegate, gattDelegate);
}

@end
