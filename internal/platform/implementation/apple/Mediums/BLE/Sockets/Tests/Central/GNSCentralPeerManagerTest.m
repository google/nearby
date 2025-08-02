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

#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Central/GNSCentralManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Central/GNSCentralPeerManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Shared/GNSWeavePacket.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

static SEL gTimerSelector = nil;
static GNSCentralPeerManager *gTimerTarget = nil;

static void ClearTimer(void) {
  gTimerSelector = nil;
  gTimerTarget = nil;
}

static void FireTimer(void) {
  NSCAssert(gTimerTarget != nil, @"The timer target cannot be nil.");
  NSCAssert(gTimerSelector != nil, @"The timer selector cannot be nil.");
  NSInvocation *invocation = [NSInvocation
      invocationWithMethodSignature:[[gTimerTarget class]
                                        instanceMethodSignatureForSelector:gTimerSelector]];
  invocation.target = gTimerTarget;
  invocation.selector = gTimerSelector;
  [invocation invoke];
  ClearTimer();
}

@interface TestGNSCentralPeerManager : GNSCentralPeerManager
@end

@implementation TestGNSCentralPeerManager

+ (NSTimer *)scheduledTimerWithTimeInterval:(NSTimeInterval)timeInterval
                                     target:(id)target
                                   selector:(SEL)selector
                                   userInfo:(nullable id)userInfo
                                    repeats:(BOOL)yesOrNo {
  NSAssert(target != nil, @"The timer target cannot be nil.");
  NSAssert(selector != nil, @"The timer selector cannot be nil.");
  NSAssert(gTimerSelector == nil, @"Timer selector already set.");
  NSAssert(gTimerTarget == nil, @"Time target already set.");
  gTimerSelector = selector;
  gTimerTarget = target;
  NSTimer *timer = OCMStrictClassMock([NSTimer class]);
  OCMStub([timer timeInterval]).andReturn(timeInterval);
  return timer;
}

@end

@interface GNSCentralPeerManagerTest : XCTestCase {
  TestGNSCentralPeerManager *_centralPeerManager;
  CBCentralManagerState _cbCentralManagerState;
  GNSCentralManager *_centralManagerMock;
  CBUUID *_serviceUUID;
  CBPeripheral *_peripheralMock;
  CBPeripheralState _peripheralState;
  NSUUID *_peripheralUUID;
  CBCharacteristic *_toPeripheralCharacteristic;
  CBCharacteristic *_fromPeripheralCharacteristic;
  CBCharacteristic *_pairingCharacteristic;
  NSArray *_characteristics;
  NSArray *_characteristicsWithPairing;
  CBService *_serviceMock;
  NSArray *_services;
  GNSSocket *_socket;
  id<GNSSocketDelegate> _socketDelegateMock;
}
@end

@implementation GNSCentralPeerManagerTest

- (void)setUp {
  _peripheralUUID = [NSUUID UUID];
  _serviceUUID = [CBUUID UUIDWithNSUUID:[NSUUID UUID]];
  _centralManagerMock = OCMStrictClassMock([GNSCentralManager class]);
  OCMStub([_centralManagerMock socketServiceUUID]).andReturn(_serviceUUID);
  _cbCentralManagerState = CBCentralManagerStatePoweredOn;
  OCMStub([_centralManagerMock cbCentralManagerState])
      .andDo(^(NSInvocation *invocation) {
        [invocation setReturnValue:&_cbCentralManagerState];
      });
  _peripheralMock = OCMStrictClassMock([CBPeripheral class]);
  OCMStub([_peripheralMock identifier]).andReturn(_peripheralUUID);
  _peripheralState = CBPeripheralStateDisconnected;
  OCMStub([_peripheralMock state])
      .andDo(^(NSInvocation *invocation) {
        [invocation setReturnValue:&_peripheralState];
      });
  OCMExpect([_peripheralMock setDelegate:[OCMArg isNotNil]]);
  _centralPeerManager = [[TestGNSCentralPeerManager alloc] initWithPeripheral:_peripheralMock
                                                               centralManager:_centralManagerMock];
  void *centralPeerManagerNonRetained = (__bridge void *)(_centralPeerManager);
  OCMStub([_peripheralMock delegate]).andReturn(centralPeerManagerNonRetained);
  _toPeripheralCharacteristic =
      [self generateCharateristicMockWithIdentifierString:@"00000100-0004-1000-8000-001A11000101"];
  _fromPeripheralCharacteristic =
      [self generateCharateristicMockWithIdentifierString:@"00000100-0004-1000-8000-001A11000102"];
  _pairingCharacteristic =
      [self generateCharateristicMockWithIdentifierString:@"17836FBD-8C6A-4B81-83CE-8560629E834B"];
  _characteristics = @[ _toPeripheralCharacteristic, _fromPeripheralCharacteristic ];
  _characteristicsWithPairing =
      @[ _toPeripheralCharacteristic, _fromPeripheralCharacteristic, _pairingCharacteristic ];
  _serviceMock = OCMStrictClassMock([CBService class]);
  OCMStub([_serviceMock UUID]).andReturn(_serviceUUID);
  _services = @[ _serviceMock ];
  OCMStub([_peripheralMock services]).andReturn(_services);
  _socketDelegateMock = OCMStrictProtocolMock(@protocol(GNSSocketDelegate));
}

- (void)tearDown {
  if (_peripheralState != CBPeripheralStateDisconnected) {
    // CBPeripharal must be disconnected when GNSCentralPeerManager is dealocated. OCM retains the
    // parameter. So |_centralManagerMock| cannot be set in the paramter in order to be deallocated
    // correctly.
    void *centralPeerManagerNonRetained = (__bridge void *)(_centralPeerManager);
    OCMExpect([_centralManagerMock
        cancelPeripheralConnectionForPeer:[OCMArg checkWithBlock:^BOOL(id obj) {
          return obj == centralPeerManagerNonRetained;
        }]]);
  }
  _centralPeerManager = nil;
  ClearTimer();
  OCMVerifyAll((id)_centralManagerMock);
  OCMVerifyAll((id)_peripheralMock);
  OCMVerifyAll((id)_toPeripheralCharacteristic);
  OCMVerifyAll((id)_fromPeripheralCharacteristic);
  OCMVerifyAll((id)_pairingCharacteristic);
  OCMVerifyAll((id)_serviceMock);
  OCMVerifyAll((id)_socketDelegateMock);
  __weak TestGNSCentralPeerManager *weakCentralPeerManager = _centralPeerManager;
  _centralPeerManager = nil;
  XCTAssertNil(weakCentralPeerManager);
}

- (NSDictionary *)peripheralConnectionOptions {
  return @{
    CBConnectPeripheralOptionNotifyOnDisconnectionKey : @YES,
#if TARGET_OS_IPHONE
      CBConnectPeripheralOptionNotifyOnConnectionKey : @YES,
      CBConnectPeripheralOptionNotifyOnNotificationKey : @YES,
#endif
  };
}

- (NSSet *)characteristicUUIDSetWithPairingUUID:(BOOL)pairing {
  NSMutableSet *uuidSet = [NSMutableSet set];
  [uuidSet addObject:_toPeripheralCharacteristic.UUID];
  [uuidSet addObject:_fromPeripheralCharacteristic.UUID];
  if (pairing) {
    [uuidSet addObject:_pairingCharacteristic.UUID];
  }
  return uuidSet;
}

- (void)checkCharacteristicsUUID:(NSArray *)uuids withPairingUUID:(BOOL)pairing {
  NSSet *uuidSet = [NSSet setWithArray:uuids];
  XCTAssertEqualObjects(uuidSet, [self characteristicUUIDSetWithPairingUUID:pairing]);
}

- (CBCharacteristic *)generateCharateristicMockWithIdentifierString:(NSString *)identifierString {
  CBCharacteristic *characteristic = OCMStrictClassMock([CBCharacteristic class]);
  CBUUID *toPeripheralCharUUID = [CBUUID UUIDWithString:identifierString];
  OCMStub([characteristic UUID]).andReturn(toPeripheralCharUUID);
  return characteristic;
}

- (void)transitionToSocketCommunicationStateWithPairingChar:(BOOL)hasPairingChar {
  OCMExpect([_centralManagerMock connectPeripheralForPeer:_centralPeerManager
                                                  options:[self peripheralConnectionOptions]]);
  [_centralPeerManager socketWithPairingCharacteristic:hasPairingChar
                                            completion:^(GNSSocket *newSocket, NSError *error) {
                                              XCTAssertNil(error);
                                              XCTAssertNotNil(newSocket);
                                              XCTAssertNil(_socket);
                                              _socket = newSocket;
                                            }];
  OCMExpect([_peripheralMock discoverServices:@[ _serviceUUID ]]);
  _peripheralState = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  OCMExpect([_peripheralMock discoverCharacteristics:[OCMArg checkWithBlock:^BOOL(id obj) {
                               [self checkCharacteristicsUUID:obj withPairingUUID:hasPairingChar];
                               return YES;
                             }]
                                          forService:_serviceMock]);
  [_centralPeerManager peripheral:_peripheralMock didDiscoverServices:nil];
  OCMStub([_serviceMock characteristics]).andReturn(_characteristicsWithPairing);
  OCMExpect([_peripheralMock setNotifyValue:YES forCharacteristic:_fromPeripheralCharacteristic]);
  [_centralPeerManager peripheral:_peripheralMock
      didDiscoverCharacteristicsForService:_serviceMock
                                     error:nil];
  GNSWeaveConnectionRequestPacket *connectionRequest =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:0
                                                             data:nil];
  OCMExpect([_peripheralMock writeValue:[connectionRequest serialize]
                      forCharacteristic:_toPeripheralCharacteristic
                                   type:CBCharacteristicWriteWithResponse]);
  [_centralPeerManager peripheral:_peripheralMock
      didUpdateNotificationStateForCharacteristic:_fromPeripheralCharacteristic
                                            error:nil];
  XCTAssertNotNil(_socket);
  _socket.delegate = _socketDelegateMock;
}

- (void)simulateConnectedSocketWithPairingChar:(BOOL)hasPairingChar {
  [self transitionToSocketCommunicationStateWithPairingChar:hasPairingChar];
  OCMExpect([_socketDelegateMock socketDidConnect:_socket]);
  GNSWeaveConnectionConfirmPacket *confirmConnection =
      [[GNSWeaveConnectionConfirmPacket alloc] initWithVersion:1 packetSize:100 data:nil];
  OCMExpect([_fromPeripheralCharacteristic value]).andReturn([confirmConnection serialize]);
  OCMExpect([_centralPeerManager.testing_connectionConfirmTimer invalidate]);
  [_centralPeerManager peripheral:_peripheralMock
      didUpdateValueForCharacteristic:_fromPeripheralCharacteristic
                                error:nil];
  // When the connection confirm packet is received the socket should be invalidated and released.
  // It's important to call |ClearTimer| here, since it retains |_centralPeerManager|.
  XCTAssertNil(_centralPeerManager.testing_connectionConfirmTimer);
  ClearTimer();
}

- (void)testCentralPeerManager {
  XCTAssertEqualObjects(_centralPeerManager.identifier, _peripheralUUID);
  // The dealloc should set the delegate to nil.
  OCMExpect([_peripheralMock setDelegate:nil]);
}

- (void)testGetSocket {
  [self simulateConnectedSocketWithPairingChar:NO];
  XCTAssertNotNil(_socket);
  // The dealloc should set the delegate to nil.
  OCMExpect([_peripheralMock setDelegate:nil]);
}

- (void)testBLEDisconnectBeforeConnect {
  OCMExpect([_centralManagerMock connectPeripheralForPeer:_centralPeerManager
                                                  options:[self peripheralConnectionOptions]]);
  [_centralPeerManager socketWithPairingCharacteristic:NO
                                            completion:^(GNSSocket *socket, NSError *error) {
                                              XCTAssertNil(socket);
                                              XCTAssertEqual(error.code, GNSErrorNoConnection);
                                            }];
  _peripheralState = CBPeripheralStateDisconnected;
  OCMExpect([_peripheralMock setDelegate:nil]);
  OCMExpect([_centralManagerMock centralPeerManagerDidDisconnect:_centralPeerManager]);
  [_centralPeerManager bleDisconnectedWithError:nil];
}

- (void)testBLEDisconnectAfterConnect {
  NSError *bleDisconnectError = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMExpect([_centralManagerMock connectPeripheralForPeer:_centralPeerManager
                                                  options:[self peripheralConnectionOptions]]);
  [_centralPeerManager socketWithPairingCharacteristic:NO
                                            completion:^(GNSSocket *socket, NSError *error) {
                                              XCTAssertEqual(error, bleDisconnectError);
                                              XCTAssertNil(socket);
                                            }];
  OCMExpect([_peripheralMock discoverServices:@[ _serviceUUID ]]);
  _peripheralState = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  _peripheralState = CBPeripheralStateDisconnected;
  OCMExpect([_peripheralMock setDelegate:nil]);
  OCMExpect([_centralManagerMock centralPeerManagerDidDisconnect:_centralPeerManager]);
  [_centralPeerManager bleDisconnectedWithError:bleDisconnectError];
}

- (void)testDiscoverServiceError {
  NSError *discoverServiceError = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMExpect([_centralManagerMock connectPeripheralForPeer:_centralPeerManager
                                                  options:[self peripheralConnectionOptions]]);
  [_centralPeerManager socketWithPairingCharacteristic:NO
                                            completion:^(GNSSocket *socket, NSError *error) {
                                              XCTAssertEqual(error, discoverServiceError);
                                              XCTAssertNil(socket);
                                            }];
  OCMExpect([_peripheralMock discoverServices:@[ _serviceUUID ]]);
  _peripheralState = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  OCMExpect([_centralManagerMock cancelPeripheralConnectionForPeer:_centralPeerManager]);
  [_centralPeerManager peripheral:_peripheralMock didDiscoverServices:discoverServiceError];
  _peripheralState = CBPeripheralStateDisconnected;
  OCMExpect([_peripheralMock setDelegate:nil]);
  OCMExpect([_centralManagerMock centralPeerManagerDidDisconnect:_centralPeerManager]);
  [_centralPeerManager bleDisconnectedWithError:nil];
}

- (void)testDiscoverCharacteristicError {
  NSError *discoverCharacteristicError = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMExpect([_centralManagerMock connectPeripheralForPeer:_centralPeerManager
                                                  options:[self peripheralConnectionOptions]]);
  [_centralPeerManager socketWithPairingCharacteristic:NO
                                            completion:^(GNSSocket *socket, NSError *error) {
                                              XCTAssertEqual(error, discoverCharacteristicError);
                                              XCTAssertNil(socket);
                                            }];
  OCMExpect([_peripheralMock discoverServices:@[ _serviceUUID ]]);
  _peripheralState = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  OCMExpect([_peripheralMock discoverCharacteristics:[OCMArg checkWithBlock:^BOOL(id obj) {
                               [self checkCharacteristicsUUID:obj withPairingUUID:NO];
                               return YES;
                             }]
                                          forService:_serviceMock]);
  [_centralPeerManager peripheral:_peripheralMock didDiscoverServices:nil];
  OCMExpect([_centralManagerMock cancelPeripheralConnectionForPeer:_centralPeerManager]);
  OCMStub([_serviceMock characteristics]).andReturn(_characteristics);
  [_centralPeerManager peripheral:_peripheralMock
      didDiscoverCharacteristicsForService:_serviceMock
                                     error:discoverCharacteristicError];
  _peripheralState = CBPeripheralStateDisconnected;
  OCMExpect([_peripheralMock setDelegate:nil]);
  OCMExpect([_centralManagerMock centralPeerManagerDidDisconnect:_centralPeerManager]);
  [_centralPeerManager bleDisconnectedWithError:nil];
}

- (void)testSetNotifyValueForCharacteristicError {
  NSError *notificationError = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMExpect([_centralManagerMock connectPeripheralForPeer:_centralPeerManager
                                                  options:[self peripheralConnectionOptions]]);
  [_centralPeerManager socketWithPairingCharacteristic:NO
                                            completion:^(GNSSocket *socket, NSError *error) {
                                              XCTAssertEqual(error, notificationError);
                                              XCTAssertNil(socket);
                                            }];
  OCMExpect([_peripheralMock discoverServices:@[ _serviceUUID ]]);
  _peripheralState = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  OCMExpect([_peripheralMock discoverCharacteristics:[OCMArg checkWithBlock:^BOOL(id obj) {
                               [self checkCharacteristicsUUID:obj withPairingUUID:NO];
                               return YES;
                             }]
                                          forService:_serviceMock]);
  [_centralPeerManager peripheral:_peripheralMock didDiscoverServices:nil];
  OCMExpect([_centralManagerMock cancelPeripheralConnectionForPeer:_centralPeerManager]);
  OCMStub([_serviceMock characteristics]).andReturn(_characteristics);
  OCMExpect([_peripheralMock setNotifyValue:YES forCharacteristic:_fromPeripheralCharacteristic]);
  [_centralPeerManager peripheral:_peripheralMock
      didDiscoverCharacteristicsForService:_serviceMock
                                     error:nil];
  [_centralPeerManager peripheral:_peripheralMock
      didUpdateNotificationStateForCharacteristic:_fromPeripheralCharacteristic
                                            error:notificationError];
  _peripheralState = CBPeripheralStateDisconnected;
  OCMExpect([_peripheralMock setDelegate:nil]);
  OCMExpect([_centralManagerMock centralPeerManagerDidDisconnect:_centralPeerManager]);
  [_centralPeerManager bleDisconnectedWithError:nil];
}

- (void)testTimeOutWaitingForConnectionResponse {
  [self transitionToSocketCommunicationStateWithPairingChar:NO];
  OCMExpect([_centralManagerMock cancelPeripheralConnectionForPeer:_centralPeerManager]);
  FireTimer();
  _peripheralState = CBPeripheralStateDisconnected;
  OCMExpect([_peripheralMock setDelegate:nil]);
  OCMExpect([_centralManagerMock centralPeerManagerDidDisconnect:_centralPeerManager]);
  OCMExpect([_socketDelegateMock socket:_socket
                 didDisconnectWithError:[OCMArg checkWithBlock:^BOOL(NSError *error) {
                   return [error.domain isEqualToString:kGNSSocketsErrorDomain] &&
                          error.code == GNSErrorConnectionTimedOut;
                 }]]);
  [_centralPeerManager bleDisconnectedWithError:nil];
  XCTAssertNil(_centralPeerManager.testing_connectionConfirmTimer);
}

- (void)testBLEDisconnectedWhileWaitingForConnectionResponse {
  [self transitionToSocketCommunicationStateWithPairingChar:NO];
  NSError *bleDisconnectError = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  _peripheralState = CBPeripheralStateDisconnected;
  OCMExpect([_peripheralMock setDelegate:nil]);
  OCMExpect([_centralManagerMock centralPeerManagerDidDisconnect:_centralPeerManager]);
  OCMExpect([_socketDelegateMock socket:_socket didDisconnectWithError:bleDisconnectError]);
  OCMExpect([_centralPeerManager.testing_connectionConfirmTimer invalidate]);
  [_centralPeerManager bleDisconnectedWithError:bleDisconnectError];
  XCTAssertNil(_centralPeerManager.testing_connectionConfirmTimer);
}

- (void)testCancelPendingSocket {
  OCMExpect([_centralManagerMock connectPeripheralForPeer:_centralPeerManager
                                                  options:[self peripheralConnectionOptions]]);
  [_centralPeerManager
      socketWithPairingCharacteristic:NO
                           completion:^(GNSSocket *socket, NSError *error) {
                             XCTAssertNil(socket);
                             XCTAssertNotNil(error);
                             XCTAssertEqualObjects(error.domain, kGNSSocketsErrorDomain);
                             XCTAssertEqual(error.code, GNSErrorCancelPendingSocketRequested);
                           }];
  OCMExpect([_peripheralMock discoverServices:@[ _serviceUUID ]]);
  _peripheralState = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  OCMExpect([_peripheralMock discoverCharacteristics:[OCMArg checkWithBlock:^BOOL(id obj) {
                               [self checkCharacteristicsUUID:obj withPairingUUID:NO];
                               return YES;
                             }]
                                          forService:_serviceMock]);
  [_centralPeerManager peripheral:_peripheralMock didDiscoverServices:nil];
  OCMExpect([_centralManagerMock cancelPeripheralConnectionForPeer:_centralPeerManager]);
  [_centralPeerManager cancelPendingSocket];
  _peripheralState = CBPeripheralStateDisconnected;
  OCMExpect([_peripheralMock setDelegate:nil]);
  OCMExpect([_centralManagerMock centralPeerManagerDidDisconnect:_centralPeerManager]);
  [_centralPeerManager bleDisconnectedWithError:nil];
}

- (void)testPairing {
  [self simulateConnectedSocketWithPairingChar:YES];
  XCTAssertNotNil(_socket);
  OCMExpect([_peripheralMock readValueForCharacteristic:_pairingCharacteristic]);
  [_centralPeerManager startBluetoothPairingWithCompletion:^(BOOL pairing, NSError *error) {
    XCTAssertTrue(pairing);
    XCTAssertNil(error);
  }];
  // The dealloc should set the delegate to nil.
  OCMExpect([_peripheralMock setDelegate:nil]);
}

- (void)testDisconnect {
  [self simulateConnectedSocketWithPairingChar:NO];
  OCMExpect([_centralManagerMock cancelPeripheralConnectionForPeer:_centralPeerManager]);
  [_socket disconnect];
  _peripheralState = CBPeripheralStateDisconnected;
  OCMExpect([_peripheralMock setDelegate:nil]);
  OCMExpect([_centralManagerMock centralPeerManagerDidDisconnect:_centralPeerManager]);
  OCMExpect([_socketDelegateMock socket:_socket didDisconnectWithError:nil]);
  [_centralPeerManager bleDisconnectedWithError:nil];
}

- (void)testDisconnectWithError {
  [self simulateConnectedSocketWithPairingChar:NO];
  OCMExpect([_centralManagerMock cancelPeripheralConnectionForPeer:_centralPeerManager]);
  [_socket disconnect];
  NSError *errorWhileBLEDisconnecting =
      [[NSError alloc] initWithDomain:@"domain" code:-42 userInfo:nil];
  _peripheralState = CBPeripheralStateDisconnected;
  OCMExpect([_peripheralMock setDelegate:nil]);
  OCMExpect([_centralManagerMock centralPeerManagerDidDisconnect:_centralPeerManager]);
  OCMExpect([_socketDelegateMock socket:_socket didDisconnectWithError:errorWhileBLEDisconnecting]);
  [_centralPeerManager bleDisconnectedWithError:errorWhileBLEDisconnecting];
  OCMVerifyAll((id)_socketDelegateMock);
}

- (void)testDropSocketWhileConnecting {
  [self transitionToSocketCommunicationStateWithPairingChar:NO];
  OCMExpect([_centralManagerMock cancelPeripheralConnectionForPeer:_centralPeerManager]);
  OCMExpect([_centralPeerManager.testing_connectionConfirmTimer invalidate]);
  OCMExpect([_peripheralMock setDelegate:nil]);
  _socket = nil;
}

@end
