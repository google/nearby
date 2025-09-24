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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCMBleConnection.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCMBleUtils.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCMConnection.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSSocket.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakeSocket.h"

static NSString *const kServiceID = @"TestServiceID";
static const NSTimeInterval kTimeout = 1.0;

@interface GNCMBleConnectionTest : XCTestCase
@end

@implementation GNCMBleConnectionTest {
  GNCFakeSocket *_fakeSocket;
  dispatch_queue_t _callbackQueue;
  GNCMBleConnection *_connection;
}

- (void)setUp {
  [super setUp];
  _fakeSocket = [[GNCFakeSocket alloc] init];
  _callbackQueue = dispatch_get_main_queue();
}

- (void)tearDown {
  _connection = nil;
  _fakeSocket = nil;
  [super tearDown];
}

- (void)testCreateConnectionWithServiceID {
  _connection = [GNCMBleConnection connectionWithSocket:(GNSSocket *)_fakeSocket
                                              serviceID:kServiceID
                                    expectedIntroPacket:NO
                                          callbackQueue:_callbackQueue];

  XCTAssertNotNil(_connection);
  XCTAssertEqual(_fakeSocket.delegate, (id<GNSSocketDelegate>)_connection);
}

- (void)testCreateConnectionWithoutServiceID {
  _connection = [GNCMBleConnection connectionWithSocket:(GNSSocket *)_fakeSocket
                                              serviceID:nil
                                    expectedIntroPacket:YES
                                          callbackQueue:_callbackQueue];

  XCTAssertNotNil(_connection);
  XCTAssertEqual(_fakeSocket.delegate, (id<GNSSocketDelegate>)_connection);
}

- (void)testSendData {
  _connection = [GNCMBleConnection connectionWithSocket:(GNSSocket *)_fakeSocket
                                              serviceID:kServiceID
                                    expectedIntroPacket:NO
                                          callbackQueue:_callbackQueue];

  NSData *data = [@"TestData" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *serviceIDHash = GNCMServiceIDHash(kServiceID);
  NSMutableData *expectedPacket = [NSMutableData dataWithData:serviceIDHash];
  [expectedPacket appendData:data];

  XCTestExpectation *sendDataExpectation = [self expectationWithDescription:@"Send data called"];
  _fakeSocket.sendDataExpectation = sendDataExpectation;

  XCTestExpectation *completionExpectation = [self expectationWithDescription:@"Send completion"];

  [_connection sendData:data
      progressHandler:^(size_t progress) {
      }
      completion:^(GNCMPayloadResult result) {
        XCTAssertEqual(result, GNCMPayloadResultSuccess);
        [completionExpectation fulfill];
      }];

  [self waitForExpectations:@[ sendDataExpectation ] timeout:kTimeout];

  XCTAssertEqualObjects(_fakeSocket.sentData, expectedPacket);
  if (_fakeSocket.sentDataCompletion) {
    _fakeSocket.sentDataCompletion(nil);
  }

  [self waitForExpectations:@[ completionExpectation ] timeout:kTimeout];
}

- (void)testSendDataEmpty {
  _connection = [GNCMBleConnection connectionWithSocket:(GNSSocket *)_fakeSocket
                                              serviceID:kServiceID
                                    expectedIntroPacket:NO
                                          callbackQueue:_callbackQueue];

  NSData *data = [NSData data];
  NSData *expectedPacket = GNCMGenerateBLEFramesIntroductionPacket(GNCMServiceIDHash(kServiceID));

  XCTestExpectation *sendDataExpectation = [self expectationWithDescription:@"Send data called"];
  _fakeSocket.sendDataExpectation = sendDataExpectation;

  XCTestExpectation *completionExpectation = [self expectationWithDescription:@"Send completion"];

  [_connection sendData:data
      progressHandler:^(size_t progress) {
      }
      completion:^(GNCMPayloadResult result) {
        XCTAssertEqual(result, GNCMPayloadResultSuccess);
        [completionExpectation fulfill];
      }];

  [self waitForExpectations:@[ sendDataExpectation ] timeout:kTimeout];

  XCTAssertEqualObjects(_fakeSocket.sentData, expectedPacket);
  if (_fakeSocket.sentDataCompletion) {
    _fakeSocket.sentDataCompletion(nil);
  }

  [self waitForExpectations:@[ completionExpectation ] timeout:kTimeout];
}

- (void)testReceiveData {
  _connection = [GNCMBleConnection connectionWithSocket:(GNSSocket *)_fakeSocket
                                              serviceID:kServiceID
                                    expectedIntroPacket:NO
                                          callbackQueue:_callbackQueue];

  NSData *expectedData = [@"TestData" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *serviceIDHash = GNCMServiceIDHash(kServiceID);
  NSMutableData *packet = [NSMutableData dataWithData:serviceIDHash];
  [packet appendData:expectedData];

  XCTestExpectation *expectation = [self expectationWithDescription:@"Payload handler"];

  GNCMConnectionHandlers *handlers = [[GNCMConnectionHandlers alloc] init];
  handlers.payloadHandler = ^(NSData *data) {
    XCTAssertEqualObjects(data, expectedData);
    [expectation fulfill];
  };
  _connection.connectionHandlers = handlers;

  [_fakeSocket simulateSocketDidReceiveData:packet];

  [self waitForExpectationsWithTimeout:kTimeout handler:nil];
}

- (void)testReceiveIntroPacketWithServiceID {
  _connection = [GNCMBleConnection connectionWithSocket:(GNSSocket *)_fakeSocket
                                              serviceID:kServiceID
                                    expectedIntroPacket:YES
                                          callbackQueue:_callbackQueue];

  NSData *introPacket = GNCMGenerateBLEFramesIntroductionPacket(GNCMServiceIDHash(kServiceID));

  XCTestExpectation *expectation = [self expectationWithDescription:@"Intro packet received"];
  expectation.inverted = YES;  // Should not trigger payload handler

  GNCMConnectionHandlers *handlers = [[GNCMConnectionHandlers alloc] init];
  handlers.payloadHandler = ^(NSData *data) {
    [expectation fulfill];
  };
  _connection.connectionHandlers = handlers;

  [_fakeSocket simulateSocketDidReceiveData:introPacket];

  // No data should be passed to the payload handler.
  [self waitForExpectationsWithTimeout:kTimeout handler:nil];
}

- (void)testReceiveIntroPacketWithoutServiceID {
  _connection = [GNCMBleConnection connectionWithSocket:(GNSSocket *)_fakeSocket
                                              serviceID:nil
                                    expectedIntroPacket:YES
                                          callbackQueue:_callbackQueue];

  NSData *introPacket = GNCMGenerateBLEFramesIntroductionPacket(GNCMServiceIDHash(kServiceID));

  XCTestExpectation *expectation = [self expectationWithDescription:@"Intro packet received"];
  expectation.inverted = YES;  // Should not trigger payload handler

  GNCMConnectionHandlers *handlers = [[GNCMConnectionHandlers alloc] init];
  handlers.payloadHandler = ^(NSData *data) {
    [expectation fulfill];
  };
  _connection.connectionHandlers = handlers;

  [_fakeSocket simulateSocketDidReceiveData:introPacket];

  // No data should be passed to the payload handler.
  [self waitForExpectationsWithTimeout:kTimeout handler:nil];

  // Now test receiving data after the intro packet.
  NSData *expectedData = [@"TestData" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *serviceIDHash = GNCMServiceIDHash(kServiceID);
  NSMutableData *packet = [NSMutableData dataWithData:serviceIDHash];
  [packet appendData:expectedData];

  XCTestExpectation *dataExpectation = [self expectationWithDescription:@"Payload handler"];

  GNCMConnectionHandlers *dataHandlers = [[GNCMConnectionHandlers alloc] init];
  dataHandlers.payloadHandler = ^(NSData *data) {
    XCTAssertEqualObjects(data, expectedData);
    [dataExpectation fulfill];
  };
  _connection.connectionHandlers = dataHandlers;

  [_fakeSocket simulateSocketDidReceiveData:packet];
  [self waitForExpectationsWithTimeout:kTimeout handler:nil];
}

- (void)testDisconnect {
  _connection = [GNCMBleConnection connectionWithSocket:(GNSSocket *)_fakeSocket
                                              serviceID:kServiceID
                                    expectedIntroPacket:NO
                                          callbackQueue:_callbackQueue];

  XCTestExpectation *expectation = [self expectationWithDescription:@"Disconnected handler"];

  GNCMConnectionHandlers *handlers = [[GNCMConnectionHandlers alloc] init];
  handlers.disconnectedHandler = ^{
    [expectation fulfill];
  };
  _connection.connectionHandlers = handlers;

  [_fakeSocket simulateSocketDidDisconnectWithError:nil];

  [self waitForExpectationsWithTimeout:kTimeout handler:nil];
}

@end
