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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralPeerManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralPeerManager.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket.h"

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Tests/Central/GNSCentralPeerManager+Testing.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Tests/Central/GNSFakeCentralManager.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Tests/Central/GNSFakePeripheral.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@interface GNSCentralPeerManagerTest : XCTestCase
@property(nonatomic) GNSFakePeripheral *fakePeripheral;
@property(nonatomic) GNSFakeCentralManager *fakeCentralManager;
@property(nonatomic) GNSCentralPeerManager *centralPeerManager;
@property(nonatomic) NSUUID *peripheralUUID;
@property(nonatomic) CBUUID *serviceUUID;
@end

@implementation GNSCentralPeerManagerTest

- (void)setUp {
  [super setUp];

  _peripheralUUID = [NSUUID UUID];
  _serviceUUID = [CBUUID UUIDWithNSUUID:[NSUUID UUID]];

  _fakePeripheral = [[GNSFakePeripheral alloc] initWithIdentifier:_peripheralUUID
                                                      serviceUUID:_serviceUUID];
  _fakeCentralManager = [[GNSFakeCentralManager alloc] initWithSocketServiceUUID:_serviceUUID];
  _fakeCentralManager.testCbManagerState = CBManagerStatePoweredOn;

  SEL selector = NSSelectorFromString(@"initWithPeripheral:centralManager:");
  NSMethodSignature *signature =
      [GNSCentralPeerManager instanceMethodSignatureForSelector:selector];
  NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
  [invocation setSelector:selector];
  GNSCentralPeerManager *peerManager = [GNSCentralPeerManager alloc];
  [invocation setTarget:peerManager];
  GNSFakePeripheral *peripheral = _fakePeripheral;
  GNSFakeCentralManager *centralManager = _fakeCentralManager;
  [invocation setArgument:&peripheral atIndex:2];
  [invocation setArgument:&centralManager atIndex:3];
  [invocation invoke];
  [invocation getReturnValue:&peerManager];
  _centralPeerManager = peerManager;
  _centralPeerManager.testing_peripheral = _fakePeripheral;
  _fakePeripheral.delegate = (id<CBPeripheralDelegate>)_centralPeerManager;
  XCTAssertNotNil(_centralPeerManager);
}

- (void)tearDown {
  _centralPeerManager = nil;
  _fakePeripheral = nil;
  _fakeCentralManager = nil;
  [super tearDown];
}

- (void)testInitialization {
  XCTAssertEqualObjects(_centralPeerManager.identifier, _peripheralUUID);
  XCTAssertEqual(_fakePeripheral.state, CBPeripheralStateDisconnected);
}

- (void)testSocketWithHandshakeData_SuccessPath {
  NSData *handshakeData = [@"handshake" dataUsingEncoding:NSUTF8StringEncoding];
  XCTestExpectation *completionExpectation =
      [self expectationWithDescription:@"Socket completion called"];

  [_centralPeerManager socketWithHandshakeData:handshakeData
                         pairingCharacteristic:NO
                                    completion:^(GNSSocket *socket, NSError *error) {
                                      XCTAssertNotNil(socket);
                                      XCTAssertNil(error);
                                      [completionExpectation fulfill];
                                    }];

  // The fake central manager will asynchronously "connect" and trigger the delegate methods.
  [self waitForExpectationsWithTimeout:2.0 handler:nil];
}

- (void)testSocketWithHandshakeData_AlreadyConnecting {
  NSData *handshakeData = [@"handshake" dataUsingEncoding:NSUTF8StringEncoding];
  XCTestExpectation *completion1Expectation =
      [self expectationWithDescription:@"Socket completion 1 called"];
  XCTestExpectation *completion2Expectation =
      [self expectationWithDescription:@"Socket completion 2 called"];

  // First call
  [_centralPeerManager socketWithHandshakeData:handshakeData
                         pairingCharacteristic:NO
                                    completion:^(GNSSocket *socket, NSError *error) {
                                      XCTAssertNotNil(socket);
                                      XCTAssertNil(error);
                                      [completion1Expectation fulfill];
                                    }];

  // Second call while pending should fail.
  [_centralPeerManager socketWithHandshakeData:handshakeData
                         pairingCharacteristic:NO
                                    completion:^(GNSSocket *socket, NSError *error) {
                                      XCTAssertNil(socket);
                                      XCTAssertNotNil(error);
                                      XCTAssertEqual(error.code, GNSErrorOperationInProgress);
                                      [completion2Expectation fulfill];
                                    }];

  [self waitForExpectations:@[ completion1Expectation, completion2Expectation ] timeout:2.0];
}

- (void)testCancelPendingSocket {
  NSData *handshakeData = [@"handshake" dataUsingEncoding:NSUTF8StringEncoding];
  XCTestExpectation *completionExpectation =
      [self expectationWithDescription:@"Socket completion called"];

  [_centralPeerManager
      socketWithHandshakeData:handshakeData
        pairingCharacteristic:NO
                   completion:^(GNSSocket *socket, NSError *error) {
                     XCTAssertNil(socket);
                     XCTAssertNotNil(error);
                     XCTAssertEqual(error.code, GNSErrorCancelPendingSocketRequested);
                     [completionExpectation fulfill];
                   }];

  [_centralPeerManager cancelPendingSocket];

  [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

- (void)testSocketWithHandshakeData_ConnectionFailed {
  NSData *handshakeData = [@"handshake" dataUsingEncoding:NSUTF8StringEncoding];
  XCTestExpectation *completionExpectation =
      [self expectationWithDescription:@"Socket completion called"];

  // Simulate a connection failure.
  _fakeCentralManager.failConnection = YES;

  [_centralPeerManager socketWithHandshakeData:handshakeData
                         pairingCharacteristic:NO
                                    completion:^(GNSSocket *socket, NSError *error) {
                                      XCTAssertNil(socket);
                                      XCTAssertNotNil(error);
                                      XCTAssertEqual(error.code, CBErrorPeripheralDisconnected);
                                      [completionExpectation fulfill];
                                    }];

  [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

- (void)testExternalDisconnectAndSocketClose {
  NSData *handshakeData = [@"handshake" dataUsingEncoding:NSUTF8StringEncoding];
  XCTestExpectation *completionExpectation =
      [self expectationWithDescription:@"Socket completion called"];

  __block GNSSocket *testSocket = nil;
  [_centralPeerManager socketWithHandshakeData:handshakeData
                         pairingCharacteristic:NO
                                    completion:^(GNSSocket *socket, NSError *error) {
                                      XCTAssertNotNil(socket);
                                      XCTAssertNil(error);
                                      testSocket = socket;
                                      [completionExpectation fulfill];
                                    }];

  // Wait for the connection to be established.
  [self waitForExpectationsWithTimeout:2.0 handler:nil];

  // Simulate an external disconnection from the central.
  [_fakeCentralManager cancelPeripheralConnectionForPeer:_centralPeerManager];
  XCTAssertEqual(_fakePeripheral.state, CBPeripheralStateDisconnected);

  // The socket should be disconnected.
  XCTAssertFalse(testSocket.isConnected);
}

#pragma mark - Helper Methods

- (void)waitForSocketOnPeerManager:(GNSCentralPeerManager *)peerManager
                           timeout:(NSTimeInterval)timeout {
  NSPredicate *socketExists =
      [NSPredicate predicateWithBlock:^BOOL(id evaluatedObject, NSDictionary *bindings) {
        return [evaluatedObject valueForKey:@"socket"] != nil;
      }];
  XCTNSPredicateExpectation *expectation =
      [[XCTNSPredicateExpectation alloc] initWithPredicate:socketExists object:peerManager];
  [self waitForExpectations:@[ expectation ] timeout:10.0];  // Increased timeout
}

- (void)establishConnectionWithPairing:(BOOL)hasPairing {
  XCTestExpectation *completionExpectation =
      [self expectationWithDescription:@"Socket completion called"];
  [_centralPeerManager socketWithHandshakeData:[@"H" dataUsingEncoding:NSUTF8StringEncoding]
                         pairingCharacteristic:hasPairing
                                    completion:^(GNSSocket *socket, NSError *error) {
                                      XCTAssertNotNil(socket);
                                      XCTAssertNil(error);
                                      [completionExpectation fulfill];
                                    }];
  [self waitForExpectationsWithTimeout:10.0 handler:nil];
  // Wait for the internal socket to be ready.
  [self waitForSocketOnPeerManager:_centralPeerManager timeout:10.0];  // Increased timeout
}

#pragma mark - RSSI Tests

- (void)testReadRSSI_NotConnected {
  XCTestExpectation *completionExpectation =
      [self expectationWithDescription:@"RSSI completion called"];
  [_centralPeerManager readRSSIWithCompletion:^(NSNumber *RSSI, NSError *error) {
    XCTAssertNil(RSSI);
    XCTAssertNotNil(error);
    XCTAssertEqual(error.code, GNSErrorNoConnection);
    [completionExpectation fulfill];
  }];
  [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

#pragma mark - Dealloc Tests

- (void)testDealloc_NotConnected {
  GNSFakePeripheral *peripheral = [[GNSFakePeripheral alloc] initWithIdentifier:[NSUUID UUID]
                                                                    serviceUUID:self.serviceUUID];
  GNSFakeCentralManager *centralManager =
      [[GNSFakeCentralManager alloc] initWithSocketServiceUUID:self.serviceUUID];
  id mockCentralManager = OCMPartialMock(centralManager);

  // Strict mock, so any call to cancelPeripheralConnectionForPeer would fail the test.

  @autoreleasepool {
    GNSCentralPeerManager *peerManager =
        [[GNSCentralPeerManager alloc] initWithPeripheral:(CBPeripheral *)peripheral
                                           centralManager:mockCentralManager
                                                    queue:dispatch_get_main_queue()];
    XCTAssertNotNil(peerManager);
    peerManager = nil;  // Trigger dealloc
  }
  OCMVerifyAll(mockCentralManager);
}

@end
