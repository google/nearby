// Copyright 2020 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralServiceManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSWeavePacket.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Tests/Peripheral/GNSFakePeripheralManager.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

static NSString *const kTestServiceUUID = @"3C672799-2B3F-4D93-9E57-29D5C5B01092";
static const NSTimeInterval kDefaultTimeout = 1.0;

@interface GNSPeripheralServiceManagerTest : XCTestCase
@end

@implementation GNSPeripheralServiceManagerTest {
  GNSPeripheralServiceManager *_serviceManager;
  GNSFakePeripheralManager *_fakePeripheralManager;
  CBUUID *_serviceUUID;
  BOOL _shouldAcceptSocket;
  GNSSocket *_receivedSocket;
  id<GNSSocketDelegate> _socketDelegateMock;
}

- (void)setUp {
  [super setUp];
  _serviceUUID = [CBUUID UUIDWithString:kTestServiceUUID];
  dispatch_queue_t queue = dispatch_queue_create("testQueue", DISPATCH_QUEUE_SERIAL);
  _fakePeripheralManager = [[GNSFakePeripheralManager alloc] initWithAdvertisedName:@"TestName"
                                                                  restoreIdentifier:nil
                                                                              queue:queue];
  _socketDelegateMock = OCMStrictProtocolMock(@protocol(GNSSocketDelegate));
  _shouldAcceptSocket = YES;

  _serviceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:_serviceUUID
                                         addPairingCharacteristic:NO
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          self->_receivedSocket = socket;
                                          socket.delegate = self->_socketDelegateMock;
                                          return self->_shouldAcceptSocket;
                                        }];
  [_serviceManager addedToPeripheralManager:_fakePeripheralManager bleServiceAddedCompletion:nil];
}

- (void)tearDown {
  OCMVerifyAll(_socketDelegateMock);
  _serviceManager = nil;
  _fakePeripheralManager = nil;
  _receivedSocket = nil;
  _serviceUUID = nil;
  [super tearDown];
}

- (void)testInitialization {
  XCTAssertEqualObjects(_serviceManager.serviceUUID, _serviceUUID);
  XCTAssertEqual(_serviceManager.cbServiceState, GNSBluetoothServiceStateNotAdded);
  XCTAssertNil(_serviceManager.cbService);
}

- (void)testServiceCreation {
  [_serviceManager willAddCBService];
  XCTAssertNotNil(_serviceManager.cbService);
  XCTAssertEqualObjects(_serviceManager.cbService.UUID, _serviceUUID);
  XCTAssertEqual(_serviceManager.cbServiceState, GNSBluetoothServiceStateAddInProgress);
  XCTAssertEqual(_serviceManager.cbService.characteristics.count, 2,
                 @"Should have incoming and outgoing characteristics");

  [_serviceManager didAddCBServiceWithError:nil];
  XCTAssertEqual(_serviceManager.cbServiceState, GNSBluetoothServiceStateAdded);
}

- (void)testServiceCreationWithPairing {
  GNSPeripheralServiceManager *serviceManager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:_serviceUUID
                                         addPairingCharacteristic:YES
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return YES;
                                        }];
  [serviceManager addedToPeripheralManager:_fakePeripheralManager bleServiceAddedCompletion:nil];
  [serviceManager willAddCBService];
  XCTAssertNotNil(serviceManager.cbService);
  XCTAssertEqual(serviceManager.cbService.characteristics.count, 3,
                 @"Should have incoming, outgoing, and pairing characteristics");
  XCTAssertNotNil(serviceManager.pairingCharacteristic);
}

#pragma mark - Socket Connection

- (CBATTRequest *)createATTRequestWithData:(NSData *)data central:(CBCentral *)central {
  CBATTRequest *request = OCMStrictClassMock([CBATTRequest class]);
  OCMStub([request value]).andReturn(data);
  OCMStub([request characteristic]).andReturn(_serviceManager.weaveIncomingCharacteristic);
  OCMStub([request central]).andReturn(central);
  return request;
}

- (CBCentral *)createCentralMock {
  NSUUID *identifier = [NSUUID UUID];
  CBCentral *central = OCMStrictClassMock([CBCentral class]);
  OCMStub([central identifier]).andReturn(identifier);
  OCMStub([central maximumUpdateValueLength]).andReturn(100);
  return central;
}

- (void)testAcceptSocket {
  _shouldAcceptSocket = YES;
  [_serviceManager willAddCBService];
  [_serviceManager didAddCBServiceWithError:nil];

  GNSWeaveConnectionRequestPacket *connectionRequest =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:100
                                                             data:nil];
  NSData *requestData = [connectionRequest serialize];
  CBCentral *centralMock = [self createCentralMock];
  CBATTRequest *request = [self createATTRequestWithData:requestData central:centralMock];

  XCTestExpectation *handlerSetExpectation =
      [self expectationWithDescription:@"Update handler set"];
  _fakePeripheralManager.updateOutgoingCharOnSocketBlock =
      ^(GNSSocket *socket, GNSUpdateValueHandler handler) {
        if (socket == self->_receivedSocket) {
          [handlerSetExpectation fulfill];
        }
      };

  XCTestExpectation *socketReadyExpectation = [self expectationWithDescription:@"Socket ready"];
  OCMStub([_socketDelegateMock socketDidConnect:[OCMArg any]]).andDo(^{
    [socketReadyExpectation fulfill];
  });

  [_serviceManager processWriteRequest:request];

  // Wait for the handler to be set asynchronously.
  [self waitForExpectations:@[ handlerSetExpectation ] timeout:kDefaultTimeout];

  XCTAssertTrue(_fakePeripheralManager.updateOutgoingCharOnSocketCalled,
                @"Handler registration should be called");
  XCTAssertNotNil(_fakePeripheralManager.serviceManagerUpdateHandler,
                  @"Update value handler should be set");

  // Now Simulate the characteristic being ready to send the confirm packet.
  [_fakePeripheralManager simulateReadyToUpdateSubscribers];

  // Wait for the socket to become ready.
  [self waitForExpectations:@[ socketReadyExpectation ] timeout:kDefaultTimeout];

  XCTAssertNotNil(_receivedSocket);
  XCTAssertEqualObjects(_receivedSocket.peerAsCentral, centralMock);

  XCTAssertTrue(_fakePeripheralManager.updateOutgoingCharOnSocketCalled,
                @"Handler registration should be called");
  XCTAssertNotNil(_fakePeripheralManager.serviceManagerUpdateHandler,
                  @"Update value handler should be set");

  // The send is async, so we can't immediately check. The fact that the socket became connected
  // implies it was sent.
  XCTAssertTrue(_fakePeripheralManager.updateOutgoingCharacteristicCalled,
                @"Data send should be called");
  XCTAssertTrue(_receivedSocket.isConnected);
  XCTAssertNotNil(_fakePeripheralManager.lastUpdatedSocket);
  XCTAssertEqualObjects(_fakePeripheralManager.lastUpdatedSocket, _receivedSocket);

  // Verify Connection Confirm packet was sent.
  XCTAssertNotNil(_fakePeripheralManager.lastUpdatedData);
  GNSWeavePacket *packet = [GNSWeavePacket parseData:_fakePeripheralManager.lastUpdatedData
                                               error:nil];
  XCTAssertTrue([packet isKindOfClass:[GNSWeaveConnectionConfirmPacket class]]);
}

- (void)testRefuseSocket {
  _shouldAcceptSocket = NO;
  [_serviceManager willAddCBService];
  [_serviceManager didAddCBServiceWithError:nil];

  GNSWeaveConnectionRequestPacket *connectionRequest =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:100
                                                             data:nil];
  NSData *requestData = [connectionRequest serialize];
  CBCentral *centralMock = [self createCentralMock];
  CBATTRequest *request = [self createATTRequestWithData:requestData central:centralMock];

  XCTestExpectation *disconnectExpectation =
      [self expectationWithDescription:@"Socket disconnect on refuse"];
  OCMExpect([_socketDelegateMock socket:[OCMArg any]
                 didDisconnectWithError:[OCMArg checkWithBlock:^BOOL(NSError *error) {
                   return error.code == GNSErrorPeripheralDidRefuseConnection;
                 }]])
      .andDo(^{
        [disconnectExpectation fulfill];
      });

  // Expect that the peripheral manager is notified of the socket disconnection.
  XCTestExpectation *pmSocketDisconnectExpectation =
      [self expectationWithDescription:@"PM socketDidDisconnect"];
  _fakePeripheralManager.socketDidDisconnectBlock = ^(GNSSocket *socket) {
    // We can capture the socket here to verify it later if needed.
    [pmSocketDisconnectExpectation fulfill];
  };

  [_serviceManager processWriteRequest:request];

  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];

  XCTAssertNotNil(_receivedSocket, @"Socket should be created even if refused");
  XCTAssertFalse(_receivedSocket.isConnected);
  XCTAssertFalse(_fakePeripheralManager.updateOutgoingCharOnSocketCalled,
                 @"Handler should not be registered");
  XCTAssertFalse(_fakePeripheralManager.updateOutgoingCharacteristicCalled,
                 @"Data should not be sent");
  XCTAssertNil(_fakePeripheralManager.lastUpdatedData, @"Should not send any data on refusal");
}

@end
