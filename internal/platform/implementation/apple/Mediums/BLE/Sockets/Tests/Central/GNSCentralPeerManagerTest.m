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

#import <CoreBluetooth/CoreBluetooth.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Central/GNSCentralManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Central/GNSCentralPeerManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSSocket+Private.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSSocket.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSWeavePacket.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Central/GNSFakeCBCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Central/GNSFakeCBPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Central/GNSFakeCBService.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Central/GNSFakeCentralManager.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

static const NSTimeInterval kDefaultTimeout = 1.0;
static const NSTimeInterval kGNSWeaveConnectionConfirmTimeout = 2.0;

#pragma mark - Fakes

@interface FakeTimer : NSProxy
@property(nonatomic, weak) id target;
@property(nonatomic) SEL selector;
@property(nonatomic) NSTimeInterval timeInterval;
@property(nonatomic, getter=isValid) BOOL valid;
- (instancetype)init;
+ (FakeTimer *)scheduledTimerWithTimeInterval:(NSTimeInterval)ti
                                       target:(id)aTarget
                                     selector:(SEL)aSelector
                                     userInfo:(id)userInfo
                                      repeats:(BOOL)yesOrNo;
+ (void)fire;
+ (void)clear;
@end

static FakeTimer *gTimer;

@implementation FakeTimer {
  __weak id _target;
  SEL _selector;
  NSTimeInterval _timeInterval;
  BOOL _valid;
}

- (instancetype)init {
  return self;
}

- (id)target {
  return _target;
}

- (void)setTarget:(id)target {
  _target = target;
}

- (SEL)selector {
  return _selector;
}

- (void)setSelector:(SEL)selector {
  _selector = selector;
}

- (NSTimeInterval)timeInterval {
  return _timeInterval;
}

- (void)setTimeInterval:(NSTimeInterval)timeInterval {
  _timeInterval = timeInterval;
}

- (BOOL)isValid {
  return _valid;
}

- (void)setValid:(BOOL)valid {
  _valid = valid;
}

+ (FakeTimer *)scheduledTimerWithTimeInterval:(NSTimeInterval)ti
                                       target:(id)aTarget
                                     selector:(SEL)aSelector
                                     userInfo:(id)userInfo
                                      repeats:(BOOL)yesOrNo {
  gTimer = [[FakeTimer alloc] init];
  gTimer.target = aTarget;
  gTimer.selector = aSelector;
  gTimer.timeInterval = ti;
  gTimer.valid = YES;
  return gTimer;
}

+ (void)fire {
  if (gTimer.valid) {
    gTimer.valid = NO;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [gTimer.target performSelector:gTimer.selector withObject:gTimer];
#pragma clang diagnostic pop
    gTimer = nil;
  }
}

+ (void)clear {
  gTimer = nil;
}

- (void)invalidate {
  self.valid = NO;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)sel {
  return [NSTimer instanceMethodSignatureForSelector:sel];
}

- (void)forwardInvocation:(NSInvocation *)invocation {
}
@end

@interface GNSFakeSocketDelegate : NSObject <GNSSocketDelegate>
@property(nonatomic) BOOL socketDidConnectCalled;
@property(nonatomic) NSError *disconnectedError;
@property(nonatomic) BOOL disconnectedCalled;
@property(nonatomic) NSData *receivedData;
@end

@implementation GNSFakeSocketDelegate
- (void)socketDidConnect:(GNSSocket *)socket {
  self.socketDidConnectCalled = YES;
}

- (void)socket:(GNSSocket *)socket didDisconnectWithError:(NSError *)error {
  self.disconnectedCalled = YES;
  self.disconnectedError = error;
}

- (void)socket:(GNSSocket *)socket didReceiveData:(NSData *)data {
  self.receivedData = data;
}
@end

#pragma mark - GNSCentralPeerManagerTest

@interface GNSCentralPeerManagerTest : XCTestCase {
  GNSCentralPeerManager *_centralPeerManager;
  GNSFakeCentralManager *_centralManager;
  CBUUID *_serviceUUID;
  GNSFakeCBPeripheral *_peripheral;
  NSUUID *_peripheralUUID;
  GNSFakeCBCharacteristic *_toPeripheralCharacteristic;
  GNSFakeCBCharacteristic *_fromPeripheralCharacteristic;
  GNSFakeCBCharacteristic *_pairingCharacteristic;
  GNSFakeCBService *_service;
  GNSSocket *_socket;
  GNSFakeSocketDelegate *_socketDelegate;
}
@end

@implementation GNSCentralPeerManagerTest

- (void)setUp {
  [super setUp];
  _peripheralUUID = [NSUUID UUID];
  _serviceUUID = [CBUUID UUIDWithNSUUID:[NSUUID UUID]];

  _centralManager = [[GNSFakeCentralManager alloc] init];
  _centralManager.socketServiceUUID = _serviceUUID;
  _centralManager.cbManagerState = CBCentralManagerStatePoweredOn;

  _peripheral = [[GNSFakeCBPeripheral alloc] init];
  _peripheral.identifier = _peripheralUUID;
  _peripheral.state = CBPeripheralStateDisconnected;

  _toPeripheralCharacteristic = [[GNSFakeCBCharacteristic alloc] init];
  _toPeripheralCharacteristic.UUID =
      [CBUUID UUIDWithString:@"00000100-0004-1000-8000-001A11000101"];
  _fromPeripheralCharacteristic = [[GNSFakeCBCharacteristic alloc] init];
  _fromPeripheralCharacteristic.UUID =
      [CBUUID UUIDWithString:@"00000100-0004-1000-8000-001A11000102"];
  _pairingCharacteristic = [[GNSFakeCBCharacteristic alloc] init];
  _pairingCharacteristic.UUID = [CBUUID UUIDWithString:@"17836FBD-8C6A-4B81-83CE-8560629E834B"];

  _service = [[GNSFakeCBService alloc] init];
  _service.UUID = _serviceUUID;
  _service.characteristics =
      @[ _toPeripheralCharacteristic, _fromPeripheralCharacteristic, _pairingCharacteristic ];
  _peripheral.services = @[ _service ];

  _centralPeerManager =
      [[GNSCentralPeerManager alloc] initWithPeripheral:(CBPeripheral *)_peripheral
                                         centralManager:(GNSCentralManager *)_centralManager];
  _socketDelegate = [[GNSFakeSocketDelegate alloc] init];
  gTimer = nil;
}

- (void)tearDown {
  [FakeTimer clear];
  [super tearDown];
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

- (void)transitionToSocketCommunicationStateWithPairingChar:(BOOL)hasPairingChar {
  XCTestExpectation *expectation = [self expectationWithDescription:@"socket completion"];
  [_centralPeerManager socketWithPairingCharacteristic:hasPairingChar
                                            completion:^(GNSSocket *newSocket, NSError *error) {
                                              XCTAssertNil(error);
                                              XCTAssertNotNil(newSocket);
                                              XCTAssertNil(_socket);
                                              _socket = newSocket;
                                              [expectation fulfill];
                                            }];
  XCTAssertTrue(_centralManager.connectPeripheralCalled);
  _peripheral.state = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  XCTAssertEqualObjects(_peripheral.discoveredServiceUUID, _serviceUUID);
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral didDiscoverServices:nil];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didDiscoverCharacteristicsForService:(CBService *)_service
                                     error:nil];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)_fromPeripheralCharacteristic
                                            error:nil];
  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
  GNSWeaveConnectionRequestPacket *connectionRequest =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:0
                                                             data:nil];
  XCTAssertEqualObjects(_peripheral.writtenData, [connectionRequest serialize]);
  XCTAssertEqual(_peripheral.writeType, CBCharacteristicWriteWithResponse);
  XCTAssertNotNil(_socket);
  _socket.delegate = _socketDelegate;
}

- (void)simulateConnectedSocketWithPairingChar:(BOOL)hasPairingChar {
  id classMock = OCMClassMock([NSTimer class]);
  OCMStub([classMock scheduledTimerWithTimeInterval:kGNSWeaveConnectionConfirmTimeout
                                             target:[OCMArg any]
                                           selector:[OCMArg anySelector]
                                           userInfo:[OCMArg any]
                                            repeats:NO])
      .ignoringNonObjectArgs()
      .andDo(^(NSInvocation *invocation) {
        NSTimeInterval interval;
        id target;
        SEL selector;
        [invocation getArgument:&interval atIndex:2];
        [invocation getArgument:&target atIndex:3];
        [invocation getArgument:&selector atIndex:4];
        FakeTimer *timer = [FakeTimer scheduledTimerWithTimeInterval:interval
                                                              target:target
                                                            selector:selector
                                                            userInfo:nil
                                                             repeats:NO];
        [invocation setReturnValue:&timer];
      });

  [self transitionToSocketCommunicationStateWithPairingChar:hasPairingChar];
  GNSWeaveConnectionConfirmPacket *confirmConnection =
      [[GNSWeaveConnectionConfirmPacket alloc] initWithVersion:1 packetSize:100 data:nil];
  _fromPeripheralCharacteristic.value = [confirmConnection serialize];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didUpdateValueForCharacteristic:(CBCharacteristic *)_fromPeripheralCharacteristic
                                error:nil];
  // When the connection confirm packet is received the socket should be invalidated and released.
  // It's important to call |ClearTimer| here, since it retains |_centralPeerManager|.
  XCTAssertNil(_centralPeerManager.testing_connectionConfirmTimer);
  [classMock stopMocking];
}

- (void)testCentralPeerManager {
  XCTAssertNotNil(_centralPeerManager);
  XCTAssertEqualObjects(_centralPeerManager.identifier, _peripheralUUID);
}

- (void)testSocketWithHandshakeData {
  id classMock = OCMClassMock([NSTimer class]);
  OCMStub([classMock scheduledTimerWithTimeInterval:kGNSWeaveConnectionConfirmTimeout
                                             target:[OCMArg any]
                                           selector:[OCMArg anySelector]
                                           userInfo:[OCMArg any]
                                            repeats:NO])
      .ignoringNonObjectArgs()
      .andDo(^(NSInvocation *invocation) {
        NSTimeInterval interval;
        id target;
        SEL selector;
        [invocation getArgument:&interval atIndex:2];
        [invocation getArgument:&target atIndex:3];
        [invocation getArgument:&selector atIndex:4];
        FakeTimer *timer = [FakeTimer scheduledTimerWithTimeInterval:interval
                                                              target:target
                                                            selector:selector
                                                            userInfo:nil
                                                             repeats:NO];
        [invocation setReturnValue:&timer];
      });

  NSData *handshakeData = [@"123" dataUsingEncoding:NSUTF8StringEncoding];
  XCTestExpectation *expectation = [self expectationWithDescription:@"socket completion"];
  [_centralPeerManager socketWithHandshakeData:handshakeData
                         pairingCharacteristic:NO
                                    completion:^(GNSSocket *newSocket, NSError *error) {
                                      XCTAssertNil(error);
                                      XCTAssertNotNil(newSocket);
                                      XCTAssertNil(_socket);
                                      _socket = newSocket;
                                      [expectation fulfill];
                                    }];
  XCTAssertTrue(_centralManager.connectPeripheralCalled);
  _peripheral.state = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  XCTAssertEqualObjects(_peripheral.discoveredServiceUUID, _serviceUUID);
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral didDiscoverServices:nil];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didDiscoverCharacteristicsForService:(CBService *)_service
                                     error:nil];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)_fromPeripheralCharacteristic
                                            error:nil];
  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
  GNSWeaveConnectionRequestPacket *connectionRequest =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:0
                                                             data:handshakeData];
  XCTAssertEqualObjects(_peripheral.writtenData, [connectionRequest serialize]);
  XCTAssertEqual(_peripheral.writeType, CBCharacteristicWriteWithResponse);
  XCTAssertNotNil(_socket);
  _socket.delegate = _socketDelegate;

  GNSWeaveConnectionConfirmPacket *confirmConnection =
      [[GNSWeaveConnectionConfirmPacket alloc] initWithVersion:1 packetSize:100 data:nil];
  _fromPeripheralCharacteristic.value = [confirmConnection serialize];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didUpdateValueForCharacteristic:(CBCharacteristic *)_fromPeripheralCharacteristic
                                error:nil];
  XCTAssertNil(_centralPeerManager.testing_connectionConfirmTimer);
  [classMock stopMocking];
}

- (void)testBLEDisconnectBeforeConnect {
  XCTestExpectation *expectation = [self expectationWithDescription:@"completion"];
  [_centralPeerManager socketWithPairingCharacteristic:NO
                                            completion:^(GNSSocket *socket, NSError *error) {
                                              XCTAssertNil(socket);
                                              XCTAssertEqual(error.code, GNSErrorNoConnection);
                                              [expectation fulfill];
                                            }];
  [_centralPeerManager bleConnect];
  _peripheral.state = CBPeripheralStateDisconnected;
  [_centralPeerManager bleDisconnectedWithError:nil];
  XCTAssertTrue(_centralManager.didDisconnectCalled);
  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
}

- (void)testBLEDisconnectAfterConnect {
  XCTestExpectation *expectation = [self expectationWithDescription:@"completion"];
  NSError *bleDisconnectError = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  [_centralPeerManager socketWithPairingCharacteristic:NO
                                            completion:^(GNSSocket *socket, NSError *error) {
                                              XCTAssertEqual(error, bleDisconnectError);
                                              XCTAssertNil(socket);
                                              [expectation fulfill];
                                            }];
  [_centralPeerManager bleConnect];
  _peripheral.state = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  _peripheral.state = CBPeripheralStateDisconnected;
  [_centralPeerManager bleDisconnectedWithError:bleDisconnectError];
  XCTAssertTrue(_centralManager.didDisconnectCalled);
  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
}

- (void)testDiscoverServiceError {
  XCTestExpectation *expectation = [self expectationWithDescription:@"completion"];
  NSError *discoverServiceError = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  [_centralPeerManager socketWithPairingCharacteristic:NO
                                            completion:^(GNSSocket *socket, NSError *error) {
                                              XCTAssertEqual(error, discoverServiceError);
                                              XCTAssertNil(socket);
                                              [expectation fulfill];
                                            }];
  [_centralPeerManager bleConnect];
  _peripheral.state = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
              didDiscoverServices:discoverServiceError];
  XCTAssertTrue(_centralManager.cancelConnectionCalled);
  _peripheral.state = CBPeripheralStateDisconnected;
  [_centralPeerManager bleDisconnectedWithError:nil];
  XCTAssertTrue(_centralManager.didDisconnectCalled);
  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
}

- (void)testDiscoverCharacteristicError {
  XCTestExpectation *expectation = [self expectationWithDescription:@"completion"];
  NSError *discoverCharacteristicError = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  [_centralPeerManager socketWithPairingCharacteristic:NO
                                            completion:^(GNSSocket *socket, NSError *error) {
                                              XCTAssertEqual(error, discoverCharacteristicError);
                                              XCTAssertNil(socket);
                                              [expectation fulfill];
                                            }];
  [_centralPeerManager bleConnect];
  _peripheral.state = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral didDiscoverServices:nil];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didDiscoverCharacteristicsForService:(CBService *)_service
                                     error:discoverCharacteristicError];
  XCTAssertTrue(_centralManager.cancelConnectionCalled);
  _peripheral.state = CBPeripheralStateDisconnected;
  [_centralPeerManager bleDisconnectedWithError:nil];
  XCTAssertTrue(_centralManager.didDisconnectCalled);
  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
}

- (void)testSetNotifyValueForCharacteristicError {
  XCTestExpectation *expectation = [self expectationWithDescription:@"completion"];
  NSError *notificationError = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  [_centralPeerManager socketWithPairingCharacteristic:NO
                                            completion:^(GNSSocket *socket, NSError *error) {
                                              XCTAssertEqual(error, notificationError);
                                              XCTAssertNil(socket);
                                              [expectation fulfill];
                                            }];
  [_centralPeerManager bleConnect];
  _peripheral.state = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral didDiscoverServices:nil];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didDiscoverCharacteristicsForService:(CBService *)_service
                                     error:nil];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)_fromPeripheralCharacteristic
                                            error:notificationError];
  XCTAssertTrue(_centralManager.cancelConnectionCalled);
  _peripheral.state = CBPeripheralStateDisconnected;
  [_centralPeerManager bleDisconnectedWithError:nil];
  XCTAssertTrue(_centralManager.didDisconnectCalled);
  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
}

- (void)testTimeOutWaitingForConnectionResponse {
  id classMock = OCMClassMock([NSTimer class]);
  OCMStub([classMock scheduledTimerWithTimeInterval:kGNSWeaveConnectionConfirmTimeout
                                             target:[OCMArg any]
                                           selector:[OCMArg anySelector]
                                           userInfo:[OCMArg any]
                                            repeats:NO])
      .ignoringNonObjectArgs()
      .andDo(^(NSInvocation *invocation) {
        NSTimeInterval interval;
        id target;
        SEL selector;
        [invocation getArgument:&interval atIndex:2];
        [invocation getArgument:&target atIndex:3];
        [invocation getArgument:&selector atIndex:4];
        FakeTimer *timer = [FakeTimer scheduledTimerWithTimeInterval:interval
                                                              target:target
                                                            selector:selector
                                                            userInfo:nil
                                                             repeats:NO];
        [invocation setReturnValue:&timer];
      });

  [self transitionToSocketCommunicationStateWithPairingChar:NO];
  [FakeTimer fire];
  XCTAssertTrue(_centralManager.cancelConnectionCalled);
  _peripheral.state = CBPeripheralStateDisconnected;
  [_centralPeerManager bleDisconnectedWithError:nil];
  XCTAssertTrue(_centralManager.didDisconnectCalled);
  XCTAssertTrue(_socketDelegate.disconnectedCalled);
  XCTAssertEqual(_socketDelegate.disconnectedError.code, GNSErrorConnectionTimedOut);
  XCTAssertNil(_centralPeerManager.testing_connectionConfirmTimer);
  [classMock stopMocking];
}

- (void)testBLEDisconnectedWhileWaitingForConnectionResponse {
  id classMock = OCMClassMock([NSTimer class]);
  OCMStub([classMock scheduledTimerWithTimeInterval:kGNSWeaveConnectionConfirmTimeout
                                             target:[OCMArg any]
                                           selector:[OCMArg anySelector]
                                           userInfo:[OCMArg any]
                                            repeats:NO])
      .ignoringNonObjectArgs()
      .andDo(^(NSInvocation *invocation) {
        NSTimeInterval interval;
        id target;
        SEL selector;
        [invocation getArgument:&interval atIndex:2];
        [invocation getArgument:&target atIndex:3];
        [invocation getArgument:&selector atIndex:4];
        FakeTimer *timer = [FakeTimer scheduledTimerWithTimeInterval:interval
                                                              target:target
                                                            selector:selector
                                                            userInfo:nil
                                                             repeats:NO];
        [invocation setReturnValue:&timer];
      });

  [self transitionToSocketCommunicationStateWithPairingChar:NO];
  NSError *bleDisconnectError = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  _peripheral.state = CBPeripheralStateDisconnected;
  [_centralPeerManager bleDisconnectedWithError:bleDisconnectError];
  XCTAssertTrue(_centralManager.didDisconnectCalled);
  XCTAssertTrue(_socketDelegate.disconnectedCalled);
  XCTAssertEqualObjects(_socketDelegate.disconnectedError, bleDisconnectError);
  XCTAssertNil(_centralPeerManager.testing_connectionConfirmTimer);
  [classMock stopMocking];
}

- (void)testCancelPendingSocket {
  XCTestExpectation *expectation = [self expectationWithDescription:@"completion"];
  [_centralPeerManager
      socketWithPairingCharacteristic:NO
                           completion:^(GNSSocket *socket, NSError *error) {
                             XCTAssertNil(socket);
                             XCTAssertNotNil(error);
                             XCTAssertEqualObjects(error.domain, kGNSSocketsErrorDomain);
                             XCTAssertEqual(error.code, GNSErrorCancelPendingSocketRequested);
                             [expectation fulfill];
                           }];
  [_centralPeerManager bleConnect];
  _peripheral.state = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral didDiscoverServices:nil];
  [_centralPeerManager cancelPendingSocket];
  XCTAssertTrue(_centralManager.cancelConnectionCalled);
  _peripheral.state = CBPeripheralStateDisconnected;
  [_centralPeerManager bleDisconnectedWithError:nil];
  XCTAssertTrue(_centralManager.didDisconnectCalled);
  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
}

- (void)testPairing {
  [self simulateConnectedSocketWithPairingChar:YES];
  XCTAssertNotNil(_socket);
  XCTestExpectation *expectation = [self expectationWithDescription:@"completion"];
  [_centralPeerManager startBluetoothPairingWithCompletion:^(BOOL pairing, NSError *error) {
    XCTAssertTrue(pairing);
    XCTAssertNil(error);
    [expectation fulfill];
  }];
  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
}

- (void)testDisconnect {
  [self simulateConnectedSocketWithPairingChar:NO];
  [_socket disconnect];
  XCTAssertTrue(_centralManager.cancelConnectionCalled);
  _peripheral.state = CBPeripheralStateDisconnected;
  [_centralPeerManager bleDisconnectedWithError:nil];
  XCTAssertTrue(_centralManager.didDisconnectCalled);
  XCTAssertTrue(_socketDelegate.disconnectedCalled);
  XCTAssertNil(_socketDelegate.disconnectedError);
}

- (void)testDisconnectWithError {
  [self simulateConnectedSocketWithPairingChar:NO];
  [_socket disconnect];
  XCTAssertTrue(_centralManager.cancelConnectionCalled);
  NSError *errorWhileBLEDisconnecting = [[NSError alloc] initWithDomain:@"domain"
                                                                   code:-42
                                                               userInfo:nil];
  _peripheral.state = CBPeripheralStateDisconnected;
  [_centralPeerManager bleDisconnectedWithError:errorWhileBLEDisconnecting];
  XCTAssertTrue(_centralManager.didDisconnectCalled);
  XCTAssertTrue(_socketDelegate.disconnectedCalled);
  XCTAssertEqualObjects(_socketDelegate.disconnectedError, errorWhileBLEDisconnecting);
}

- (void)testDropSocketWhileConnecting {
  id classMock = OCMClassMock([NSTimer class]);
  OCMStub([classMock scheduledTimerWithTimeInterval:kGNSWeaveConnectionConfirmTimeout
                                             target:[OCMArg any]
                                           selector:[OCMArg anySelector]
                                           userInfo:[OCMArg any]
                                            repeats:NO])
      .ignoringNonObjectArgs()
      .andDo(^(NSInvocation *invocation) {
        NSTimeInterval interval;
        id target;
        SEL selector;
        [invocation getArgument:&interval atIndex:2];
        [invocation getArgument:&target atIndex:3];
        [invocation getArgument:&selector atIndex:4];
        FakeTimer *timer = [FakeTimer scheduledTimerWithTimeInterval:interval
                                                              target:target
                                                            selector:selector
                                                            userInfo:nil
                                                             repeats:NO];
        [invocation setReturnValue:&timer];
      });

  [self transitionToSocketCommunicationStateWithPairingChar:NO];
  _socket = nil;
  XCTAssertTrue(_centralManager.cancelConnectionCalled);
  XCTAssertFalse(gTimer.isValid);
  [classMock stopMocking];
}

- (void)testReadRSSIWhenConnected {
  [self simulateConnectedSocketWithPairingChar:NO];
  XCTestExpectation *expectation = [self expectationWithDescription:@"RSSI completion"];
  [_centralPeerManager readRSSIWithCompletion:^(NSNumber *RSSI, NSError *error) {
    XCTAssertEqualObjects(RSSI, @(-50));
    XCTAssertNil(error);
    [expectation fulfill];
  }];
  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
}

- (void)testReadRSSIWhenDisconnected {
  XCTestExpectation *expectation = [self expectationWithDescription:@"RSSI completion"];
  [_centralPeerManager readRSSIWithCompletion:^(NSNumber *RSSI, NSError *error) {
    XCTAssertNil(RSSI);
    XCTAssertEqual(error.code, GNSErrorNoConnection);
    [expectation fulfill];
  }];
  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
}

- (void)testHandleErrorPacket {
  [self simulateConnectedSocketWithPairingChar:NO];
  XCTAssertNotNil(_socket);

  XCTestExpectation *expectation = [self expectationWithDescription:@"socket disconnected"];
  _socketDelegate.disconnectedCalled = NO;
  GNSWeaveErrorPacket *errorPacket = [[GNSWeaveErrorPacket alloc] initWithPacketCounter:1];
  _fromPeripheralCharacteristic.value = [errorPacket serialize];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didUpdateValueForCharacteristic:(CBCharacteristic *)_fromPeripheralCharacteristic
                                error:nil];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
    if (_socketDelegate.disconnectedCalled) {
      [expectation fulfill];
    } else {
      _peripheral.state = CBPeripheralStateDisconnected;
      [_centralPeerManager bleDisconnectedWithError:nil];
      if (_socketDelegate.disconnectedCalled) {
        [expectation fulfill];
      }
    }
  });
  [self waitForExpectationsWithTimeout:kDefaultTimeout * 2 handler:nil];
  XCTAssertTrue(_socketDelegate.disconnectedCalled);
  XCTAssertEqual(_socketDelegate.disconnectedError.code, GNSErrorWeaveErrorPacketReceived);
}

- (void)testHandleDataPacket {
  [self simulateConnectedSocketWithPairingChar:NO];
  XCTAssertNotNil(_socket);

  NSData *data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  GNSWeaveDataPacket *dataPacket = [[GNSWeaveDataPacket alloc] initWithPacketCounter:1
                                                                         firstPacket:YES
                                                                          lastPacket:YES
                                                                                data:data];
  _fromPeripheralCharacteristic.value = [dataPacket serialize];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didUpdateValueForCharacteristic:(CBCharacteristic *)_fromPeripheralCharacteristic
                                error:nil];
  XCTAssertEqualObjects(_socketDelegate.receivedData, data);
}

- (void)testHandleDataPacketTransferInProgress {
  [self simulateConnectedSocketWithPairingChar:NO];
  XCTAssertNotNil(_socket);

  // Send first part of data to put socket in waitingForIncomingData state.
  NSData *data1 = [@"part1" dataUsingEncoding:NSUTF8StringEncoding];
  GNSWeaveDataPacket *dataPacket1 = [[GNSWeaveDataPacket alloc] initWithPacketCounter:1
                                                                          firstPacket:YES
                                                                           lastPacket:NO
                                                                                 data:data1];
  _fromPeripheralCharacteristic.value = [dataPacket1 serialize];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didUpdateValueForCharacteristic:(CBCharacteristic *)_fromPeripheralCharacteristic
                                error:nil];
  XCTAssertNil(_socketDelegate.receivedData);
  XCTAssertFalse(_socketDelegate.disconnectedCalled);

  // Send another first packet, should trigger GNSErrorWeaveDataTransferInProgress.
  NSData *data2 = [@"part2" dataUsingEncoding:NSUTF8StringEncoding];
  GNSWeaveDataPacket *dataPacket2 = [[GNSWeaveDataPacket alloc] initWithPacketCounter:2
                                                                          firstPacket:YES
                                                                           lastPacket:NO
                                                                                 data:data2];
  _fromPeripheralCharacteristic.value = [dataPacket2 serialize];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didUpdateValueForCharacteristic:(CBCharacteristic *)_fromPeripheralCharacteristic
                                error:nil];

  XCTestExpectation *expectation = [self expectationWithDescription:@"socket disconnected"];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
    if (_socketDelegate.disconnectedCalled) {
      [expectation fulfill];
    } else {
      _peripheral.state = CBPeripheralStateDisconnected;
      [_centralPeerManager bleDisconnectedWithError:nil];
      if (_socketDelegate.disconnectedCalled) {
        [expectation fulfill];
      }
    }
  });
  [self waitForExpectationsWithTimeout:kDefaultTimeout * 2 handler:nil];
  XCTAssertTrue(_socketDelegate.disconnectedCalled);
  XCTAssertEqual(_socketDelegate.disconnectedError.code, GNSErrorWeaveDataTransferInProgress);
}

- (void)testCbManagerStateDidUpdate {
  // 1. Set BT to off
  _centralManager.cbManagerState = CBCentralManagerStatePoweredOff;

  // 2. Try to connect, this will transition state to BleConnecting but not call connectPeripheral
  [_centralPeerManager socketWithPairingCharacteristic:NO
                                            completion:^(GNSSocket *socket, NSError *error){
                                            }];
  XCTAssertFalse(_centralManager.connectPeripheralCalled);

  // 3. Set BT to on and notify manager
  _centralManager.cbManagerState = CBCentralManagerStatePoweredOn;
  [_centralPeerManager cbManagerStateDidUpdate];

  // 4. Verify connectPeripheral was called now
  XCTAssertTrue(_centralManager.connectPeripheralCalled);
}

- (void)testIsBLEConnected {
  // Initial state: NotConnected, should be NO.
  XCTAssertFalse([_centralPeerManager isBLEConnected]);

  // Transition to BleConnecting
  [_centralPeerManager bleConnect];
  XCTAssertFalse([_centralPeerManager isBLEConnected]);

  // Transition to BleConnected/DiscoveringService
  _peripheral.state = CBPeripheralStateConnected;
  [_centralPeerManager bleConnected];
  XCTAssertTrue([_centralPeerManager isBLEConnected]);
  XCTAssertEqualObjects(_peripheral.discoveredServiceUUID, _serviceUUID);

  // Transition to DiscoveringCharacteristic
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral didDiscoverServices:nil];
  XCTAssertTrue([_centralPeerManager isBLEConnected]);

  // Transition to SettingNotifications
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didDiscoverCharacteristicsForService:(CBService *)_service
                                     error:nil];
  XCTAssertTrue([_centralPeerManager isBLEConnected]);

  // Transition to SocketCommunication
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)_fromPeripheralCharacteristic
                                            error:nil];
  XCTAssertTrue([_centralPeerManager isBLEConnected]);

  // Transition to BleDisconnecting
  [_centralPeerManager disconnectSocket:_socket];
  XCTAssertTrue([_centralPeerManager isBLEConnected]);

  // Transition to NotConnected
  _peripheral.state = CBPeripheralStateDisconnected;
  [_centralPeerManager bleDisconnectedWithError:nil];
  XCTAssertFalse([_centralPeerManager isBLEConnected]);
}

- (void)testCleanRSSICompletionAfterDisconnection {
  [self simulateConnectedSocketWithPairingChar:NO];
  _peripheral.shouldCallDidReadRSSI = NO;

  XCTestExpectation *expectation = [self expectationWithDescription:@"RSSI completion"];
  [_centralPeerManager readRSSIWithCompletion:^(NSNumber *RSSI, NSError *error) {
    XCTAssertNil(RSSI);
    XCTAssertEqual(error.code, GNSErrorNoConnection);
    [expectation fulfill];
  }];

  [_socket disconnect];
  _peripheral.state = CBPeripheralStateDisconnected;
  [_centralPeerManager bleDisconnectedWithError:nil];

  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
}

- (void)testDidWriteValueForCharacteristicNoError {
  [self simulateConnectedSocketWithPairingChar:NO];
  XCTestExpectation *expectation = [self expectationWithDescription:@"write completion"];
  NSData *data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  [(id<GNSSocketOwner>)_centralPeerManager sendData:data
                                             socket:_socket
                                         completion:^(NSError *error) {
                                           XCTAssertNil(error);
                                           [expectation fulfill];
                                         }];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didWriteValueForCharacteristic:(CBCharacteristic *)_toPeripheralCharacteristic
                               error:nil];
  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
}

- (void)testDidWriteValueForCharacteristicError {
  [self simulateConnectedSocketWithPairingChar:NO];
  XCTestExpectation *expectation = [self expectationWithDescription:@"write completion"];
  NSData *data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  NSError *writeError = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  [(id<GNSSocketOwner>)_centralPeerManager sendData:data
                                             socket:_socket
                                         completion:^(NSError *error) {
                                           XCTAssertEqualObjects(error, writeError);
                                           [expectation fulfill];
                                         }];
  [_centralPeerManager peripheral:(CBPeripheral *)_peripheral
      didWriteValueForCharacteristic:(CBCharacteristic *)_toPeripheralCharacteristic
                               error:writeError];
  [self waitForExpectationsWithTimeout:kDefaultTimeout handler:nil];
  XCTAssertTrue(_centralManager.cancelConnectionCalled);
}

- (void)testSocketMaximumUpdateValueLength {
  [self simulateConnectedSocketWithPairingChar:NO];
  XCTAssertEqual([(id<GNSSocketOwner>)_centralPeerManager socketMaximumUpdateValueLength:_socket],
                 100);
}

- (void)testHandleConnectionRequestPacket {
  [self simulateConnectedSocketWithPairingChar:NO];
  XCTAssertNotNil(_socket);

  XCTestExpectation *expectation = [self expectationWithDescription:@"socket disconnected"];
  _socketDelegate.disconnectedCalled = NO;
  GNSWeaveConnectionRequestPacket *packet =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:0
                                                             data:nil];
  [(id<GNSWeavePacketHandler>)_centralPeerManager handleConnectionRequestPacket:packet context:nil];

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
    if (_socketDelegate.disconnectedCalled) {
      [expectation fulfill];
    } else {
      _peripheral.state = CBPeripheralStateDisconnected;
      [_centralPeerManager bleDisconnectedWithError:nil];
      if (_socketDelegate.disconnectedCalled) {
        [expectation fulfill];
      }
    }
  });
  [self waitForExpectationsWithTimeout:kDefaultTimeout * 2 handler:nil];
  XCTAssertTrue(_socketDelegate.disconnectedCalled);
  XCTAssertEqual(_socketDelegate.disconnectedError.code, GNSErrorUnexpectedWeaveControlPacket);
}

@end
