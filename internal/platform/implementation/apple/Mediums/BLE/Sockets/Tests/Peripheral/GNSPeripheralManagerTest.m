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

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Peripheral/GNSPeripheralManager.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Peripheral/GNSPeripheralManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Peripheral/GNSPeripheralServiceManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Peripheral/GNSPeripheralServiceManager.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSSocket+Private.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSSocket.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Peripheral/GNSFakePeripheralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Peripheral/GNSFakePeripheralServiceManager.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@interface GNSPeripheralManager () {
 @public
  NSMutableDictionary<NSUUID *, NSMutableArray<GNSUpdateValueHandler> *>
      *_handlerQueuePerSocketIdentifier;
  NSDictionary<NSString *, id> *_advertisementInProgressData;
  NSDictionary<NSString *, id> *_advertisementData;
}
@end

@interface TestGNSPeripheralManager : GNSPeripheralManager
@property(nonatomic, strong) GNSFakePeripheralManager *fakeCBPeripheralManager;
@property(nonatomic, strong) NSDictionary<id, id> *cbOptions;
@property(nonatomic) XCTestExpectation *stateUpdateExpectation;
@end

@implementation TestGNSPeripheralManager

- (CBPeripheralManager *)cbPeripheralManagerWithDelegate:(id<CBPeripheralManagerDelegate>)delegate
                                                   queue:(dispatch_queue_t)queue
                                                 options:(NSDictionary<NSString *, id> *)options {
  _cbOptions = options;
  self.fakeCBPeripheralManager = [[GNSFakePeripheralManager alloc] initWithDelegate:delegate
                                                                              queue:queue
                                                                            options:options];
  self.fakeCBPeripheralManager.delegate = delegate;
  return (CBPeripheralManager *)self.fakeCBPeripheralManager;
}

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheralManager {
  [super peripheralManagerDidUpdateState:peripheralManager];
  [_stateUpdateExpectation fulfill];
  _stateUpdateExpectation = nil;
}

@end

@interface GNSPeripheralManagerTest : XCTestCase {
  TestGNSPeripheralManager *_peripheralManager;
  NSString *_displayName;
  NSString *_restoreIdentifier;
}
@end

@implementation GNSPeripheralManagerTest

- (void)setUp {
  _displayName = @"DisplayName";
  _restoreIdentifier = @"RestoreIdentifier";
  _peripheralManager =
      [[TestGNSPeripheralManager alloc] initWithAdvertisedName:_displayName
                                             restoreIdentifier:_restoreIdentifier
                                                         queue:dispatch_get_main_queue()];
  XCTAssertFalse(_peripheralManager.isStarted);
}

- (void)tearDown {
  if (_peripheralManager.isStarted) {
    [_peripheralManager stop];
  }
  if (_peripheralManager.cbPeripheralManager) {
#if TARGET_OS_IPHONE
    NSDictionary *expectedOptions =
        @{CBPeripheralManagerOptionRestoreIdentifierKey : _restoreIdentifier};
    XCTAssertEqualObjects(expectedOptions, _peripheralManager.cbOptions);
#endif
  }
}

- (CBATTRequest *)requestWithCharacteristic:(CBCharacteristic *)characteristic {
  id central = OCMClassMock([CBCentral class]);
  OCMStub([central identifier]).andReturn([NSUUID UUID]);
  id request = OCMClassMock([CBATTRequest class]);
  OCMStub([request central]).andReturn(central);
  OCMStub([request characteristic]).andReturn(characteristic);
  return request;
}

#pragma mark - Init

- (void)testInitWithAdvertisedNameAndRestoreIdentifier {
  _peripheralManager = [[TestGNSPeripheralManager alloc] initWithAdvertisedName:_displayName
                                                              restoreIdentifier:_restoreIdentifier];
  XCTAssertNotNil(_peripheralManager);
  XCTAssertFalse(_peripheralManager.isStarted);
}

- (void)testDescription {
  NSString *description = [_peripheralManager description];
  XCTAssertTrue([description containsString:@"GNSPeripheralManager"]);
  XCTAssertTrue([description containsString:@"started NO"]);

  [_peripheralManager start];
  description = [_peripheralManager description];
  XCTAssertTrue([description containsString:@"GNSPeripheralManager"]);
  XCTAssertTrue([description containsString:@"started YES"]);
}

#pragma mark - Start/Stop

- (void)startPeripheralManagerWithPeripheralManagerState:(CBPeripheralManagerState)state
                               expectedAdvertisementData:
                                   (NSDictionary<NSString *, id> *)expectedAdvertisement {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:state];
  [self checkAdvertisementData:expectedAdvertisement];
}

- (void)testStartTwice {
  [_peripheralManager start];
  XCTAssertTrue(_peripheralManager.isStarted);
  [_peripheralManager start];
  XCTAssertTrue(_peripheralManager.isStarted);
}

- (void)updatePeripheralManagerWithPeripheralManagerState:(CBPeripheralManagerState)state {
  XCTestExpectation *exp = [self expectationWithDescription:@"state update"];
  _peripheralManager.stateUpdateExpectation = exp;
  _peripheralManager.fakeCBPeripheralManager.state = state;
  [_peripheralManager peripheralManagerDidUpdateState:(CBPeripheralManager *)_peripheralManager
                                                          .fakeCBPeripheralManager];
  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  XCTAssertTrue(_peripheralManager.isStarted);
}

- (void)checkAdvertisementData:(NSDictionary<NSString *, id> *)expectedAdvertisement {
  if (expectedAdvertisement) {
    XCTAssertEqualObjects(expectedAdvertisement[CBAdvertisementDataLocalNameKey],
                          _peripheralManager.fakeCBPeripheralManager
                              .advertisementData[CBAdvertisementDataLocalNameKey]);
    NSSet<CBUUID *> *expectedUUIDs =
        [NSSet setWithArray:expectedAdvertisement[CBAdvertisementDataServiceUUIDsKey]];
    NSSet<CBUUID *> *advertisedUUIDs =
        [NSSet setWithArray:_peripheralManager.fakeCBPeripheralManager
                                .advertisementData[CBAdvertisementDataServiceUUIDsKey]];
    XCTAssertEqualObjects(expectedUUIDs, advertisedUUIDs);
  } else {
    XCTAssertNil(_peripheralManager.fakeCBPeripheralManager.advertisementData);
  }
}

- (void)updatePeripheralManagerWithPeripheralManagerState:(CBPeripheralManagerState)state
                                expectedAdvertisementData:
                                    (NSDictionary<NSString *, id> *)expectedAdvertisement {
  _peripheralManager.fakeCBPeripheralManager.state = state;
  [_peripheralManager peripheralManagerDidUpdateState:(CBPeripheralManager *)_peripheralManager
                                                          .fakeCBPeripheralManager];
  XCTAssertTrue(_peripheralManager.isStarted);
  if (expectedAdvertisement) {
    XCTAssertEqualObjects(expectedAdvertisement[CBAdvertisementDataLocalNameKey],
                          _peripheralManager.fakeCBPeripheralManager
                              .advertisementData[CBAdvertisementDataLocalNameKey]);
    NSSet<CBUUID *> *expectedUUIDs =
        [NSSet setWithArray:expectedAdvertisement[CBAdvertisementDataServiceUUIDsKey]];
    NSSet<CBUUID *> *advertisedUUIDs =
        [NSSet setWithArray:_peripheralManager.fakeCBPeripheralManager
                                .advertisementData[CBAdvertisementDataServiceUUIDsKey]];
    XCTAssertEqualObjects(expectedUUIDs, advertisedUUIDs);
  } else {
    XCTAssertNil(_peripheralManager.fakeCBPeripheralManager.advertisementData);
  }
}

// Starts with bluetooth off.

- (void)testStartWithNoServiceBluetoothResetting {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateResetting];
  XCTAssertEqual(_peripheralManager.fakeCBPeripheralManager.removeAllServicesCount, 1);
  [self checkAdvertisementData:nil];
}

- (void)testStartWithNoServiceBluetoothUnknown {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateUnknown];
  XCTAssertEqual(_peripheralManager.fakeCBPeripheralManager.removeAllServicesCount, 1);
  [self checkAdvertisementData:nil];
}

- (void)testStartWithNoServiceBluetoothUnauthorized {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateUnauthorized];
  XCTAssertEqual(_peripheralManager.fakeCBPeripheralManager.removeAllServicesCount, 1);
  [self checkAdvertisementData:nil];
}

- (void)testStartWithNoServiceBluetoothUnsupported {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateUnsupported];
  [self checkAdvertisementData:nil];
}

- (void)testStartWithNoServiceBluetoothOff {
  NSDictionary<NSString *, id> *expectedAdvertisingData = nil;
  [self startPeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOff
                               expectedAdvertisementData:expectedAdvertisingData];
}

// Starts with bluetooth on: should advertise local name on BLE.
- (void)testStartWithNoServiceBluetoothOn {
  NSDictionary<NSString *, id> *expectedAdvertisingData = nil;
  [self startPeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                               expectedAdvertisementData:expectedAdvertisingData];
}

// Starts with BLE unknown, and then turn on bluetooth. Advertisement should be done only after
// turning on bluetooth.
- (void)testStartWithBleStateUnknownAndTurnOnBluetooth {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateUnknown];
  [self checkAdvertisementData:nil];
  XCTAssertEqual(_peripheralManager.fakeCBPeripheralManager.removeAllServicesCount, 1);

  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                                expectedAdvertisementData:nil];
}

- (void)testStartWithBleStateResettingAndTurnOnBluetooth {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateResetting];
  [self checkAdvertisementData:nil];
  XCTAssertEqual(_peripheralManager.fakeCBPeripheralManager.removeAllServicesCount, 1);

  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                                expectedAdvertisementData:nil];
}

#pragma mark - Restore

#if TARGET_OS_IPHONE
- (void)testRestoreState {
  [_peripheralManager start];
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"FEF3"];
  GNSFakePeripheralServiceManager *serviceManager =
      [[GNSFakePeripheralServiceManager alloc] initWithServiceUUID:serviceUUID];

  XCTestExpectation *addServiceExp = [self expectationWithDescription:@"add service"];
  [_peripheralManager addPeripheralServiceManager:serviceManager
                        bleServiceAddedCompletion:^(NSError *error) {
                          [addServiceExp fulfill];
                        }];
  [self waitForExpectationsWithTimeout:1.0 handler:nil];

  CBMutableService *service = ((GNSPeripheralServiceManager *)serviceManager).cbService;
  XCTAssertNotNil(service);
  serviceManager.advertising = YES;

  NSDictionary<NSString *, id> *advertisementData = @{
    CBAdvertisementDataServiceUUIDsKey : @[ serviceUUID ],
    CBAdvertisementDataLocalNameKey : _displayName
  };
  NSDictionary<NSString *, id> *restoreState = @{
    CBPeripheralManagerRestoredStateServicesKey : @[ service ],
    CBPeripheralManagerRestoredStateAdvertisementDataKey : advertisementData
  };
  [_peripheralManager
      peripheralManager:(CBPeripheralManager *)_peripheralManager.fakeCBPeripheralManager
       willRestoreState:restoreState];

  XCTAssertTrue(serviceManager.restored);
  [self checkAdvertisementData:nil];

  _peripheralManager.fakeCBPeripheralManager.state = CBPeripheralManagerStatePoweredOn;
  [_peripheralManager peripheralManagerDidUpdateState:(CBPeripheralManager *)_peripheralManager
                                                          .fakeCBPeripheralManager];
  [self checkAdvertisementData:nil];
}
#endif  // TARGET_OS_IPHONE

#pragma mark - Advertising

- (void)testAdvertisingWithOneService {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                                expectedAdvertisementData:nil];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"FEF3"];
  GNSFakePeripheralServiceManager *serviceManager =
      [[GNSFakePeripheralServiceManager alloc] initWithServiceUUID:serviceUUID];
  XCTestExpectation *addServiceExp = [self expectationWithDescription:@"add service"];
  [_peripheralManager addPeripheralServiceManager:serviceManager
                        bleServiceAddedCompletion:^(NSError *error) {
                          [addServiceExp fulfill];
                        }];
  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  serviceManager.advertising = YES;
  [self checkAdvertisementData:nil];
}

- (void)testAdvertisingWithTwoServices {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                                expectedAdvertisementData:nil];

  CBUUID *serviceUUID1 = [CBUUID UUIDWithString:@"FEF3"];
  GNSFakePeripheralServiceManager *serviceManager1 =
      [[GNSFakePeripheralServiceManager alloc] initWithServiceUUID:serviceUUID1];
  XCTestExpectation *addServiceExp1 = [self expectationWithDescription:@"add service 1"];
  [_peripheralManager addPeripheralServiceManager:serviceManager1
                        bleServiceAddedCompletion:^(NSError *error) {
                          [addServiceExp1 fulfill];
                        }];
  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  serviceManager1.advertising = YES;
  [self checkAdvertisementData:nil];

  CBUUID *serviceUUID2 = [CBUUID UUIDWithString:@"FEF4"];
  GNSFakePeripheralServiceManager *serviceManager2 =
      [[GNSFakePeripheralServiceManager alloc] initWithServiceUUID:serviceUUID2];
  XCTestExpectation *addServiceExp2 = [self expectationWithDescription:@"add service 2"];
  [_peripheralManager addPeripheralServiceManager:serviceManager2
                        bleServiceAddedCompletion:^(NSError *error) {
                          [addServiceExp2 fulfill];
                        }];
  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  serviceManager2.advertising = YES;
  [self checkAdvertisementData:nil];
}

- (void)testStopAdvertisingService {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                                expectedAdvertisementData:nil];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"FEF3"];
  GNSFakePeripheralServiceManager *serviceManager =
      [[GNSFakePeripheralServiceManager alloc] initWithServiceUUID:serviceUUID];
  XCTestExpectation *addServiceExp = [self expectationWithDescription:@"add service"];
  [_peripheralManager addPeripheralServiceManager:serviceManager
                        bleServiceAddedCompletion:^(NSError *error) {
                          [addServiceExp fulfill];
                        }];
  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  serviceManager.advertising = YES;
  [self checkAdvertisementData:nil];

  serviceManager.advertising = NO;
  [self checkAdvertisementData:nil];
}

#pragma mark - Subscribe/unsubscribe characteristics

- (void)testSubscribeUnsubscribe {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                                expectedAdvertisementData:nil];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"FEF3"];
  GNSFakePeripheralServiceManager *serviceManager =
      [[GNSFakePeripheralServiceManager alloc] initWithServiceUUID:serviceUUID];
  XCTestExpectation *addServiceExp = [self expectationWithDescription:@"add service"];
  [_peripheralManager addPeripheralServiceManager:serviceManager
                        bleServiceAddedCompletion:^(NSError *error) {
                          [addServiceExp fulfill];
                        }];
  [self waitForExpectationsWithTimeout:1.0 handler:nil];

  CBCentral *central = OCMClassMock([CBCentral class]);
  OCMStub(central.identifier).andReturn([NSUUID UUID]);
  CBCharacteristic *characteristic =
      ((GNSPeripheralServiceManager *)serviceManager).cbService.characteristics[0];
  [_peripheralManager
                 peripheralManager:(CBPeripheralManager *)_peripheralManager.fakeCBPeripheralManager
                           central:central
      didSubscribeToCharacteristic:characteristic];
  XCTAssertEqual(serviceManager.subscribedCentrals.count, 1);
  XCTAssertTrue([serviceManager.subscribedCentrals containsObject:central]);

  [_peripheralManager peripheralManager:(CBPeripheralManager *)
                                            _peripheralManager.fakeCBPeripheralManager
                                central:central
       didUnsubscribeFromCharacteristic:characteristic];
  XCTAssertEqual(serviceManager.subscribedCentrals.count, 0);
}

- (void)testRemovePeripheralServiceManager {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                                expectedAdvertisementData:nil];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"FEF3"];
  GNSFakePeripheralServiceManager *serviceManager =
      [[GNSFakePeripheralServiceManager alloc] initWithServiceUUID:serviceUUID];
  XCTestExpectation *addServiceExp = [self expectationWithDescription:@"add service"];
  [_peripheralManager addPeripheralServiceManager:serviceManager
                        bleServiceAddedCompletion:^(NSError *error) {
                          [addServiceExp fulfill];
                        }];
  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  XCTAssertEqual(_peripheralManager.fakeCBPeripheralManager.services.count, 1);

  XCTestExpectation *removeServiceExp = [self expectationWithDescription:@"remove service"];
  [_peripheralManager removePeripheralServiceManagerForServiceUUID:serviceUUID
                                       bleServiceRemovedCompletion:^(NSError *error) {
                                         XCTAssertNil(error);
                                         [removeServiceExp fulfill];
                                       }];
  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  XCTAssertEqual(_peripheralManager.fakeCBPeripheralManager.services.count, 0);
}

- (void)testRemoveNonExistingPeripheralServiceManager {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                                expectedAdvertisementData:nil];

  XCTestExpectation *removeServiceExp = [self expectationWithDescription:@"remove service"];
  [_peripheralManager removePeripheralServiceManagerForServiceUUID:[CBUUID UUIDWithString:@"FEF3"]
                                       bleServiceRemovedCompletion:^(NSError *error) {
                                         XCTAssertNil(error);
                                         [removeServiceExp fulfill];
                                       }];
  [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

#pragma mark - Update outgoing characteristic

- (void)testUpdateOutgoingCharWithHandlerReturningYes {
  id socketMock = OCMClassMock([GNSSocket class]);
  NSUUID *socketID = [NSUUID UUID];
  OCMStub([socketMock socketIdentifier]).andReturn(socketID);

  __block int handlerCallCount = 0;
  [_peripheralManager updateOutgoingCharOnSocket:socketMock
                                     withHandler:^BOOL {
                                       handlerCallCount++;
                                       return YES;
                                     }];

  XCTAssertEqual(handlerCallCount, 1);
  XCTAssertNil(_peripheralManager->_handlerQueuePerSocketIdentifier[socketID]);
}

- (void)testUpdateOutgoingCharWithHandlerReturningNo {
  id socketMock = OCMClassMock([GNSSocket class]);
  NSUUID *socketID = [NSUUID UUID];
  OCMStub([socketMock socketIdentifier]).andReturn(socketID);

  __block int handlerCallCount = 0;
  GNSUpdateValueHandler handler = ^BOOL {
    handlerCallCount++;
    return NO;
  };
  [_peripheralManager updateOutgoingCharOnSocket:socketMock withHandler:handler];

  XCTAssertEqual(handlerCallCount, 1);
  XCTAssertEqual(_peripheralManager->_handlerQueuePerSocketIdentifier[socketID].count, 1);
}

- (void)testUpdateOutgoingCharQueueExists {
  id socketMock = OCMClassMock([GNSSocket class]);
  NSUUID *socketID = [NSUUID UUID];
  OCMStub([socketMock socketIdentifier]).andReturn(socketID);

  __block int handlerCallCount = 0;
  GNSUpdateValueHandler handler = ^BOOL {
    handlerCallCount++;
    return NO;
  };
  [_peripheralManager updateOutgoingCharOnSocket:socketMock withHandler:handler];
  [_peripheralManager updateOutgoingCharOnSocket:socketMock withHandler:handler];

  XCTAssertEqual(handlerCallCount, 1);
  XCTAssertEqual(_peripheralManager->_handlerQueuePerSocketIdentifier[socketID].count, 2);
}

- (void)testPeripheralManagerIsReadyToUpdateSubscribers {
  id socketMock = OCMClassMock([GNSSocket class]);
  NSUUID *socketID = [NSUUID UUID];
  OCMStub([socketMock socketIdentifier]).andReturn(socketID);

  __block int handler1CallCount = 0;
  __block BOOL handler1ReturnValue = NO;
  GNSUpdateValueHandler handler1 = ^BOOL {
    handler1CallCount++;
    return handler1ReturnValue;
  };
  [_peripheralManager updateOutgoingCharOnSocket:socketMock withHandler:handler1];
  XCTAssertEqual(handler1CallCount, 1);
  XCTAssertEqual(_peripheralManager->_handlerQueuePerSocketIdentifier[socketID].count, 1);

  __block int handler2CallCount = 0;
  GNSUpdateValueHandler handler2 = ^BOOL {
    handler2CallCount++;
    return YES;
  };
  [_peripheralManager updateOutgoingCharOnSocket:socketMock withHandler:handler2];
  XCTAssertEqual(_peripheralManager->_handlerQueuePerSocketIdentifier[socketID].count, 2);

  // When peripheralManagerIsReadyToUpdateSubscribers is called, handler1 is called again.
  // It returns NO, so handler2 should not be called, and handler1 should remain in queue.
  [_peripheralManager
      peripheralManagerIsReadyToUpdateSubscribers:(CBPeripheralManager *)
                                                      _peripheralManager.fakeCBPeripheralManager];
  XCTAssertEqual(handler1CallCount, 2);
  XCTAssertEqual(handler2CallCount, 0);
  XCTAssertEqual(_peripheralManager->_handlerQueuePerSocketIdentifier[socketID].count, 2);

  // Set handler1 to return YES. When peripheralManagerIsReadyToUpdateSubscribers is called,
  // handler1 is called, returns YES, and is removed. Then handler2 is called, returns YES,
  // and is removed. The queue should be empty.
  handler1ReturnValue = YES;
  [_peripheralManager
      peripheralManagerIsReadyToUpdateSubscribers:(CBPeripheralManager *)
                                                      _peripheralManager.fakeCBPeripheralManager];
  XCTAssertEqual(handler1CallCount, 3);
  XCTAssertEqual(handler2CallCount, 1);
  XCTAssertNil(_peripheralManager->_handlerQueuePerSocketIdentifier[socketID]);
}


- (void)testUpdateOutgoingCharacteristicWithoutCentral {
  id socketMock = OCMClassMock([GNSSocket class]);
  id serviceManagerMock = OCMClassMock([GNSPeripheralServiceManager class]);
  OCMStub([socketMock owner]).andReturn(serviceManagerMock);
  OCMStub([socketMock peerAsCentral]).andReturn(nil);

  NSData *data = [@"test" dataUsingEncoding:NSUTF8StringEncoding];
  BOOL result = [_peripheralManager updateOutgoingCharacteristic:data onSocket:socketMock];

  XCTAssertFalse(result);
}

#pragma mark - CBPeripheralManagerDelegate callbacks

- (void)testPeripheralManagerDidStartAdvertisingWithError {
  _peripheralManager->_advertisementInProgressData = @{@"key" : @"value"};
  NSError *error = [NSError errorWithDomain:@"test" code:0 userInfo:nil];
  [_peripheralManager
      peripheralManagerDidStartAdvertising:(CBPeripheralManager *)
                                               _peripheralManager.fakeCBPeripheralManager
                                     error:error];
  XCTAssertNil(_peripheralManager->_advertisementInProgressData);
  XCTAssertNil(_peripheralManager->_advertisementData);
}

- (void)testPeripheralManagerDidStartAdvertisingNoError {
  NSDictionary<NSString *, id> *advertisementData = @{
    CBAdvertisementDataServiceUUIDsKey : @[ [CBUUID UUIDWithString:@"FEF3"] ],
    CBAdvertisementDataLocalNameKey : _displayName
  };
  _peripheralManager->_advertisementInProgressData = advertisementData;
  _peripheralManager.fakeCBPeripheralManager.advertising = YES;

  id peripheralManagerMock = OCMPartialMock(_peripheralManager);
  OCMExpect([peripheralManagerMock updateAdvertisedServices]);

  [_peripheralManager
      peripheralManagerDidStartAdvertising:(CBPeripheralManager *)
                                               _peripheralManager.fakeCBPeripheralManager
                                     error:nil];

  XCTAssertNil(_peripheralManager->_advertisementInProgressData);
  XCTAssertEqualObjects(_peripheralManager->_advertisementData, advertisementData);
  OCMVerifyAll(peripheralManagerMock);
}

- (void)testSocketDidDisconnect {
  id socketMock = OCMClassMock([GNSSocket class]);
  NSUUID *socketID = [NSUUID UUID];
  OCMStub([socketMock socketIdentifier]).andReturn(socketID);
  OCMStub([socketMock isConnected]).andReturn(NO);

  __block int handler1CallCount = 0;
  GNSUpdateValueHandler handler1 = ^BOOL {
    handler1CallCount++;
    return NO;
  };
  [_peripheralManager updateOutgoingCharOnSocket:socketMock withHandler:handler1];
  XCTAssertEqual(handler1CallCount, 1);
  XCTAssertEqual(_peripheralManager->_handlerQueuePerSocketIdentifier[socketID].count, 1);

  __block int handler2CallCount = 0;
  GNSUpdateValueHandler handler2 = ^BOOL {
    handler2CallCount++;
    return YES;
  };
  [_peripheralManager updateOutgoingCharOnSocket:socketMock withHandler:handler2];
  XCTAssertEqual(_peripheralManager->_handlerQueuePerSocketIdentifier[socketID].count, 2);
  XCTAssertEqual(handler2CallCount, 0);  // handler2 shouldn't be called here.

  [_peripheralManager socketDidDisconnect:socketMock];

  XCTAssertEqual(handler1CallCount, 2);
  XCTAssertEqual(handler2CallCount, 1);
  XCTAssertNil(_peripheralManager->_handlerQueuePerSocketIdentifier[socketID]);
}

#pragma mark - Did receive write requests

- (void)testDidReceiveWriteRequestsSuccess {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                                expectedAdvertisementData:nil];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"FEF3"];
  GNSFakePeripheralServiceManager *serviceManager =
      [[GNSFakePeripheralServiceManager alloc] initWithServiceUUID:serviceUUID];
  id serviceManagerMock = OCMPartialMock(serviceManager);

  XCTestExpectation *addServiceExp = [self expectationWithDescription:@"add service"];
  [_peripheralManager addPeripheralServiceManager:serviceManager
                        bleServiceAddedCompletion:^(NSError *error) {
                          [addServiceExp fulfill];
                        }];
  [self waitForExpectationsWithTimeout:1.0 handler:nil];

  CBATTRequest *request =
      [self requestWithCharacteristic:serviceManager.cbService.characteristics[0]];
  OCMExpect([serviceManagerMock canProcessWriteRequest:request]).andReturn(CBATTErrorSuccess);
  OCMExpect([serviceManagerMock processWriteRequest:request]);

  [_peripheralManager
            peripheralManager:(CBPeripheralManager *)_peripheralManager.fakeCBPeripheralManager
      didReceiveWriteRequests:@[ request ]];

  XCTAssertEqualObjects(_peripheralManager.fakeCBPeripheralManager.lastRespondRequest, request);
  XCTAssertEqual(_peripheralManager.fakeCBPeripheralManager.lastRespondResult, CBATTErrorSuccess);
  OCMVerifyAll(serviceManagerMock);
}

- (void)testDidReceiveWriteRequestsFailure {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                                expectedAdvertisementData:nil];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"FEF3"];
  GNSFakePeripheralServiceManager *serviceManager =
      [[GNSFakePeripheralServiceManager alloc] initWithServiceUUID:serviceUUID];
  id serviceManagerMock = OCMPartialMock(serviceManager);

  XCTestExpectation *addServiceExp = [self expectationWithDescription:@"add service"];
  [_peripheralManager addPeripheralServiceManager:serviceManager
                        bleServiceAddedCompletion:^(NSError *error) {
                          [addServiceExp fulfill];
                        }];
  [self waitForExpectationsWithTimeout:1.0 handler:nil];

  CBATTRequest *request =
      [self requestWithCharacteristic:serviceManager.cbService.characteristics[0]];
  OCMExpect([serviceManagerMock canProcessWriteRequest:request])
      .andReturn(CBATTErrorRequestNotSupported);
  OCMReject([serviceManagerMock processWriteRequest:request]);

  [_peripheralManager
            peripheralManager:(CBPeripheralManager *)_peripheralManager.fakeCBPeripheralManager
      didReceiveWriteRequests:@[ request ]];

  XCTAssertEqualObjects(_peripheralManager.fakeCBPeripheralManager.lastRespondRequest, request);
  XCTAssertEqual(_peripheralManager.fakeCBPeripheralManager.lastRespondResult,
                 CBATTErrorRequestNotSupported);
  OCMVerifyAll(serviceManagerMock);
}

@end
