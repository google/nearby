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

#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Peripheral/GNSPeripheralManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Peripheral/GNSPeripheralServiceManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Shared/GNSSocket+Private.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@interface TestGNSPeripheralManager : GNSPeripheralManager
@property(nonatomic, strong) id cbPeripheralManagerMock;
@property(nonatomic, strong) NSDictionary *cbOptions;
@end

@implementation TestGNSPeripheralManager

- (CBPeripheralManager *)cbPeripheralManagerWithDelegate:(id<CBPeripheralManagerDelegate>)delegate
                                                   queue:(dispatch_queue_t)queue
                                                 options:(NSDictionary *)options {
  _cbOptions = options;
  return _cbPeripheralManagerMock;
}

@end

@interface GNSPeripheralManagerTest : XCTestCase {
  TestGNSPeripheralManager *_peripheralManager;
  NSString *_displayName;
  NSString *_restoreIdentifier;
  id _cbPeripheralManagerMock;
  NSDictionary *_cbAdvertisementData;
  CBPeripheralManagerState _cbPeripheralManagerState;
  NSMutableArray *_mocksToVerify;
  NSMutableDictionary *_cbServiceStatePerPeripheral;
}
@end

@implementation GNSPeripheralManagerTest

- (void)setUp {
  _mocksToVerify = [NSMutableArray array];
  _cbServiceStatePerPeripheral = [NSMutableDictionary dictionary];
  _displayName = @"DisplayName";
  _restoreIdentifier = @"RestoreIdentifier";
  _cbPeripheralManagerMock = OCMStrictClassMock([CBPeripheralManager class]);
  [_mocksToVerify addObject:_cbPeripheralManagerMock];
  OCMStub([_cbPeripheralManagerMock state]).andDo(^(NSInvocation *invocation) {
    [invocation setReturnValue:&_cbPeripheralManagerState];
  });
  OCMStub([_cbPeripheralManagerMock isAdvertising]).andDo(^(NSInvocation *invocation) {
    BOOL isAdvertising = _cbAdvertisementData != nil;
    [invocation setReturnValue:&isAdvertising];
  });
  OCMStub([_cbPeripheralManagerMock stopAdvertising]).andDo(^(NSInvocation *invocation) {
    _cbAdvertisementData = nil;
  });
  OCMStub([_cbPeripheralManagerMock
      startAdvertising:[OCMArg checkWithBlock:^BOOL(id obj) {
        _cbAdvertisementData = obj;
        [_peripheralManager peripheralManagerDidStartAdvertising:_cbPeripheralManagerMock
                                                           error:nil];
        return YES;
      }]]);
  _peripheralManager = [[TestGNSPeripheralManager alloc]
      initWithAdvertisedName:_displayName
           restoreIdentifier:_restoreIdentifier
                       queue:dispatch_get_main_queue()];
  _peripheralManager.cbPeripheralManagerMock = _cbPeripheralManagerMock;
  XCTAssertFalse(_peripheralManager.isStarted);
}

- (void)tearDown {
  if (_peripheralManager.isStarted) {
    [self stopPeripheralManager];
  }
  if (_peripheralManager.cbPeripheralManager) {
    NSDictionary *expectedOptions =
        @{CBPeripheralManagerOptionRestoreIdentifierKey : _restoreIdentifier};
    XCTAssertEqualObjects(expectedOptions, _peripheralManager.cbOptions);
    OCMVerifyAll(_peripheralManager.cbPeripheralManagerMock);
  }
  for (id mock in _mocksToVerify) {
    OCMVerifyAll(mock);
  }
}

#pragma mark - Start/Stop

- (void)startPeripheralManagerWithPeripheralManagerState:(CBPeripheralManagerState)state
                               expectedAdvertisementData:(NSDictionary *)expectedAdvertisement {
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:state];
  [self checkAdvertisementData:expectedAdvertisement];
}

- (void)updatePeripheralManagerWithPeripheralManagerState:(CBPeripheralManagerState)state {
  _cbPeripheralManagerState = state;
  [_peripheralManager peripheralManagerDidUpdateState:_cbPeripheralManagerMock];
  XCTAssertTrue(_peripheralManager.isStarted);
}

- (void)checkAdvertisementData:(NSDictionary *)expectedAdvertisement {
  if (expectedAdvertisement) {
    XCTAssertEqualObjects(expectedAdvertisement[CBAdvertisementDataLocalNameKey],
                          _cbAdvertisementData[CBAdvertisementDataLocalNameKey]);
    NSSet *expectedUUIDs =
        [NSSet setWithArray:expectedAdvertisement[CBAdvertisementDataServiceUUIDsKey]];
    NSSet *advertisedUUIDs =
        [NSSet setWithArray:_cbAdvertisementData[CBAdvertisementDataServiceUUIDsKey]];
    XCTAssertEqualObjects(expectedUUIDs, advertisedUUIDs);
  } else {
    XCTAssertNil(_cbAdvertisementData);
  }
}

- (void)updatePeripheralManagerWithPeripheralManagerState:(CBPeripheralManagerState)state
                                expectedAdvertisementData:(NSDictionary *)expectedAdvertisement {
  _cbPeripheralManagerState = state;
  [_peripheralManager peripheralManagerDidUpdateState:_cbPeripheralManagerMock];
  XCTAssertTrue(_peripheralManager.isStarted);
  if (expectedAdvertisement) {
    XCTAssertEqualObjects(expectedAdvertisement[CBAdvertisementDataLocalNameKey],
                          _cbAdvertisementData[CBAdvertisementDataLocalNameKey]);
    NSSet *expectedUUIDs =
        [NSSet setWithArray:expectedAdvertisement[CBAdvertisementDataServiceUUIDsKey]];
    NSSet *advertisedUUIDs =
        [NSSet setWithArray:_cbAdvertisementData[CBAdvertisementDataServiceUUIDsKey]];
    XCTAssertEqualObjects(expectedUUIDs, advertisedUUIDs);
  } else {
    XCTAssertNil(_cbAdvertisementData);
  }
}

- (void)stopPeripheralManager {
  OCMExpect([_cbPeripheralManagerMock removeAllServices]);
  OCMExpect([_cbPeripheralManagerMock setDelegate:nil]);
  [_peripheralManager stop];
  XCTAssertNil(_cbAdvertisementData);
  XCTAssertFalse(_peripheralManager.isStarted);
}

// Starts with bluetooth off.

- (void)testStartWithNoServiceBluetoothResetting {
  [_peripheralManager start];
  OCMExpect([_peripheralManager.cbPeripheralManager removeAllServices]);
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateResetting];
  [self checkAdvertisementData:nil];
}

- (void)testStartWithNoServiceBluetoothUnknown {
  [_peripheralManager start];
  OCMExpect([_peripheralManager.cbPeripheralManager removeAllServices]);
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateUnknown];
  [self checkAdvertisementData:nil];
}

- (void)testStartWithNoServiceBluetoothUnauthorized {
  [_peripheralManager start];
  OCMExpect([_peripheralManager.cbPeripheralManager removeAllServices]);
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateUnauthorized];
  [self checkAdvertisementData:nil];
}

- (void)testStartWithNoServiceBluetoothUnsupported {
  OCMStub([_cbPeripheralManagerMock isAdvertising]).andReturn(NO);
  [self startPeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateUnsupported
                               expectedAdvertisementData:nil];
}

- (void)testStartWithNoServiceBluetoothOff {
  NSDictionary *expectedAdvertisingData = @{
    CBAdvertisementDataServiceUUIDsKey : @[],
    CBAdvertisementDataLocalNameKey : _displayName,
  };
  [self startPeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOff
                               expectedAdvertisementData:expectedAdvertisingData];
}

// Starts with bluetooth on: should advertise local name on BLE.
- (void)testStartWithNoServiceBluetoothOn {
  NSDictionary *expectedAdvertisingData = @{
    CBAdvertisementDataServiceUUIDsKey : @[],
    CBAdvertisementDataLocalNameKey : _displayName,
  };
  [self startPeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                               expectedAdvertisementData:expectedAdvertisingData];
}

// Starts with BLE unknown, and then turn on bluetooth. Advertisement should be done only after
// turning on bluetooth.
- (void)testStartWithBleStateUnknownAndTurnOnBluetooth {
  [_peripheralManager start];
  OCMExpect([_peripheralManager.cbPeripheralManager removeAllServices]);
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateUnknown];
  [self checkAdvertisementData:nil];

  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                                expectedAdvertisementData:@{
                                  CBAdvertisementDataServiceUUIDsKey : @[],
                                  CBAdvertisementDataLocalNameKey : _displayName,
                                }];
}

- (void)testStartWithBleStateResettingAndTurnOnBluetooth {
  [_peripheralManager start];
  OCMExpect([_peripheralManager.cbPeripheralManager removeAllServices]);
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateUnknown];
  [self checkAdvertisementData:nil];

  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn
                                expectedAdvertisementData:@{
                                  CBAdvertisementDataServiceUUIDsKey : @[],
                                  CBAdvertisementDataLocalNameKey : _displayName,
                                }];
}

#pragma mark - Restore

- (void)testRestore {
  [_peripheralManager start];
  GNSPeripheralServiceManager *service1 =
      [self peripheralServiceManagerWithServiceState:GNSBluetoothServiceStateAdded];
  OCMStub([service1 isAdvertising]).andReturn(YES);
  [_peripheralManager addPeripheralServiceManager:service1 bleServiceAddedCompletion:nil];
  GNSPeripheralServiceManager *service2 =
      [self peripheralServiceManagerWithServiceState:GNSBluetoothServiceStateAdded];
  OCMStub([service2 isAdvertising]).andReturn(YES);
  [_peripheralManager addPeripheralServiceManager:service2 bleServiceAddedCompletion:nil];
  CBMutableService *restoredCBService1 = OCMStrictClassMock([CBMutableService class]);
  OCMStub([restoredCBService1 UUID]).andReturn(service1.serviceUUID);
  CBMutableService *restoredCBService2 = OCMStrictClassMock([CBMutableService class]);
  OCMStub([restoredCBService2 UUID]).andReturn(service2.serviceUUID);
  NSDictionary *state = @{
    CBPeripheralManagerRestoredStateAdvertisementDataKey : @"Name",
    CBPeripheralManagerRestoredStateServicesKey : @[ restoredCBService1, restoredCBService2 ],
  };
  OCMExpect([service1 restoredCBService:restoredCBService1]);
  OCMExpect([service2 restoredCBService:restoredCBService2]);
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock willRestoreState:state];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn];
  [self checkAdvertisementData:@{
    CBAdvertisementDataServiceUUIDsKey : @[ service1.serviceUUID, service2.serviceUUID ],
    CBAdvertisementDataLocalNameKey : _displayName,
  }];
}

#pragma mark - Advertising

// Generates a peripheral service manager mock ready to be added to |_peripheralManager|.
- (GNSPeripheralServiceManager *)peripheralServiceManager {
  return [self peripheralServiceManagerWithServiceState:GNSBluetoothServiceStateNotAdded];
}

// Generates a peripheral service manager mock |_peripheralManager| with the cbServiceState
// set to |initialServiceState|.
- (GNSPeripheralServiceManager *)peripheralServiceManagerWithServiceState:
        (GNSBluetoothServiceState)initialServiceState {
  CBUUID *uuid = [CBUUID UUIDWithNSUUID:[NSUUID UUID]];
  CBMutableService *cbService = OCMClassMock([CBMutableService class]);
  OCMStub([cbService UUID]).andReturn(uuid);
  [_mocksToVerify addObject:cbService];
  GNSPeripheralServiceManager *peripheralServiceManager =
      OCMStrictClassMock([GNSPeripheralServiceManager class]);
  [_mocksToVerify addObject:peripheralServiceManager];
  OCMStub([peripheralServiceManager cbService]).andReturn(cbService);
  OCMStub([peripheralServiceManager serviceUUID]).andReturn(uuid);
  OCMExpect([peripheralServiceManager addedToPeripheralManager:_peripheralManager
                                     bleServiceAddedCompletion:[OCMArg any]]);

  __block GNSBluetoothServiceState cbServiceState = initialServiceState;
  OCMStub([peripheralServiceManager cbServiceState]).andDo(^(NSInvocation *invocation) {
    GNSBluetoothServiceState stateToReturn = cbServiceState;
    return [invocation setReturnValue:&stateToReturn];
  });
  OCMStub([peripheralServiceManager willAddCBService]).andDo(^(NSInvocation *invocation) {
    cbServiceState = GNSBluetoothServiceStateAddInProgress;
  });

  OCMStub([peripheralServiceManager didAddCBServiceWithError:[OCMArg any]])
      .andDo(^(NSInvocation *invocation) {
        __unsafe_unretained NSError *error = nil;
        [invocation getArgument:&error atIndex:2];
        if (!error) {
          cbServiceState = GNSBluetoothServiceStateAdded;
        } else {
          cbServiceState = GNSBluetoothServiceStateNotAdded;
        }
      });
  OCMStub([peripheralServiceManager didRemoveCBService]).andDo(^(NSInvocation *invocation) {
    cbServiceState = GNSBluetoothServiceStateNotAdded;
  });
  return peripheralServiceManager;
}

// Adds a peripheral service manager mock into |_peripheralManager|.
- (void)addPeripheralManager:(id)peripheralServiceManager {
  CBMutableService *service = [peripheralServiceManager cbService];
  OCMExpect([_cbPeripheralManagerMock addService:service]);
  [_peripheralManager addPeripheralServiceManager:peripheralServiceManager
                        bleServiceAddedCompletion:nil];
}

// Adds one peripheral service manager.
- (void)testAddOneServiceStartsAdvertisment {
  GNSPeripheralServiceManager *peripheralServiceManager = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager isAdvertising]).andReturn(YES);
  // Starting before the services are added should not start adverting.
  [self addPeripheralManager:peripheralServiceManager];
  [_peripheralManager start];
  XCTAssertEqual(GNSBluetoothServiceStateNotAdded, [peripheralServiceManager cbServiceState]);
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn];
  XCTAssertEqual(GNSBluetoothServiceStateAddInProgress, [peripheralServiceManager cbServiceState]);
  [self checkAdvertisementData:nil];
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                          didAddService:[peripheralServiceManager cbService]
                                  error:nil];
  XCTAssertEqual(GNSBluetoothServiceStateAdded, [peripheralServiceManager cbServiceState]);

  // Adding the service should start the advertisment.
  [self checkAdvertisementData:@{
    CBAdvertisementDataServiceUUIDsKey : @[ [peripheralServiceManager serviceUUID] ],
    CBAdvertisementDataLocalNameKey : _displayName,
  }];
}

// Adds one peripheral service manager fails with error.
- (void)testAddOneServiceError {
  GNSPeripheralServiceManager *peripheralServiceManager = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager isAdvertising]).andReturn(YES);
  [self addPeripheralManager:peripheralServiceManager];
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn];
  NSError *expectedError = [[NSError alloc] initWithDomain:@"Test" code:-42 userInfo:nil];
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                          didAddService:[peripheralServiceManager cbService]
                                  error:expectedError];
  [self checkAdvertisementData:nil];
  XCTAssertEqual(GNSBluetoothServiceStateNotAdded, [peripheralServiceManager cbServiceState]);
}

// Adds two peripheral service managers, one that advertise itself, and one that doesn't. Only
// one service should be advertised.
- (void)testAddTwoServices {
  GNSPeripheralServiceManager *peripheralServiceManager1 = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager1 isAdvertising]).andReturn(NO);
  [self addPeripheralManager:peripheralServiceManager1];
  GNSPeripheralServiceManager *peripheralServiceManager2 = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager2 isAdvertising]).andReturn(YES);
  [self addPeripheralManager:peripheralServiceManager2];
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn];
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                          didAddService:[peripheralServiceManager1 cbService]
                                  error:nil];
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                          didAddService:[peripheralServiceManager2 cbService]
                                  error:nil];
  NSDictionary *expectedAdvertisement = @{
    CBAdvertisementDataServiceUUIDsKey : @[ [peripheralServiceManager2 serviceUUID] ],
    CBAdvertisementDataLocalNameKey : _displayName,
  };
  [self checkAdvertisementData:expectedAdvertisement];
}

// Adds one peripheral service manager while not advertising. Changes the advertising value for this
// peripheral service manager.
- (void)testAddServiceAndChangeAdvertisingValue {
  GNSPeripheralServiceManager *peripheralServiceManager = [self peripheralServiceManager];
  OCMStubRecorder *isAdvertisingStub = OCMStub([peripheralServiceManager isAdvertising]);
  isAdvertisingStub.andReturn(NO);
  [self addPeripheralManager:peripheralServiceManager];
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn];
  [self checkAdvertisementData:nil];
  XCTAssertEqual(GNSBluetoothServiceStateAddInProgress, [peripheralServiceManager cbServiceState]);
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                          didAddService:[peripheralServiceManager cbService]
                                  error:nil];
  [self checkAdvertisementData:@{
    CBAdvertisementDataServiceUUIDsKey : @[],
    CBAdvertisementDataLocalNameKey : _displayName,
  }];
  isAdvertisingStub.andReturn(YES);
  [_peripheralManager updateAdvertisedServices];
  [self checkAdvertisementData:@{
    CBAdvertisementDataServiceUUIDsKey : @[ [peripheralServiceManager serviceUUID] ],
    CBAdvertisementDataLocalNameKey : _displayName,
  }];
  return;
}

- (void)startWithPeripheralServiceManagers:(NSArray *)managers {
  NSMutableArray *advertisedServiceUUIDs = [NSMutableArray array];
  for (GNSPeripheralServiceManager *peripheralServiceManager in managers) {
    [self addPeripheralManager:peripheralServiceManager];
    if (peripheralServiceManager.isAdvertising) {
      [advertisedServiceUUIDs addObject:peripheralServiceManager.serviceUUID];
    }
  }
  [_peripheralManager start];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOn];
  for (GNSPeripheralServiceManager *peripheralServiceManager in managers) {
    [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                            didAddService:[peripheralServiceManager cbService]
                                    error:nil];
  }
  [self checkAdvertisementData:@{
    CBAdvertisementDataServiceUUIDsKey : advertisedServiceUUIDs,
    CBAdvertisementDataLocalNameKey : _displayName,
  }];
}

#pragma mark - Subscribe/unsubscribe characteristics

// Creates a characteristic mock, and adds it into |_mocksToVerify|. If no service is provided,
// a service mock is created (added into |_mocksToVerify| too). The service uuid is attach
// to the service. If no service uuid is provided, an uuid is created.
- (CBMutableCharacteristic *)prepareCharacteristicForCBService:(CBService *)cbService
                                               withServiceUUID:(CBUUID *)serviceUUID {
  if (!serviceUUID) {
    serviceUUID = [CBUUID UUIDWithNSUUID:[NSUUID UUID]];
  }
  if (!cbService) {
    cbService = OCMClassMock([CBService class]);
    OCMStub([cbService UUID]).andReturn(serviceUUID);
    [_mocksToVerify addObject:cbService];
  }
  CBMutableCharacteristic *characteristicMock = OCMClassMock([CBMutableCharacteristic class]);
  OCMStub([characteristicMock service]).andReturn(cbService);
  [_mocksToVerify addObject:characteristicMock];
  return characteristicMock;
}

// Creates a characteristic mock based on the service from the peripheral service manager.
// The mock is added into _mocksToVerify.
- (CBMutableCharacteristic *)prepareCharacteristicForPeripheralServiceManager:
        (GNSPeripheralServiceManager *)peripheralServiceManager {
  CBService *cbService = peripheralServiceManager.cbService;
  CBUUID *serviceUUID = cbService.UUID;
  return [self prepareCharacteristicForCBService:cbService withServiceUUID:serviceUUID];
}

// Adds a peripheral service manager, and a central subscribes to its service.
- (void)testCentralDidSubscribe {
  id peripheralServiceManager = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager isAdvertising]).andReturn(YES);
  [self startWithPeripheralServiceManagers:@[ peripheralServiceManager ]];
  id centralMock = OCMClassMock([CBCentral class]);
  CBMutableCharacteristic *characteristicMock =
      [self prepareCharacteristicForPeripheralServiceManager:peripheralServiceManager];
  OCMExpect([peripheralServiceManager central:centralMock
                 didSubscribeToCharacteristic:characteristicMock]);
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                                central:centralMock
           didSubscribeToCharacteristic:characteristicMock];
  OCMVerifyAll(centralMock);
}

// Adds a peripheral service manager, and a central unsubscribes to its service.
- (void)testCentralDidUnsubscribe {
  id peripheralServiceManager = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager isAdvertising]).andReturn(YES);
  [self startWithPeripheralServiceManagers:@[ peripheralServiceManager ]];
  id centralMock = OCMClassMock([CBCentral class]);
  CBMutableCharacteristic *characteristicMock =
      [self prepareCharacteristicForPeripheralServiceManager:peripheralServiceManager];
  OCMExpect([peripheralServiceManager central:centralMock
             didUnsubscribeFromCharacteristic:characteristicMock]);

  // As a workaourd for b/31752176 We are restarting the peripheral manager after the central
  // unsubscribe.
  OCMExpect([_cbPeripheralManagerMock removeAllServices]);
  OCMExpect([_cbPeripheralManagerMock setDelegate:nil]);

  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                                central:centralMock
       didUnsubscribeFromCharacteristic:characteristicMock];
  OCMVerifyAll(centralMock);
}

#pragma mark - Characteristic requests

// Creates an ATT request mock based on the characteristic. The mock is added into |_mocksToVerify|
- (CBATTRequest *)prepareRequestForCharacteristic:(CBMutableCharacteristic *)characteristic {
  id requestMock = OCMClassMock([CBATTRequest class]);
  OCMStub([requestMock characteristic]).andReturn(characteristic);
  [_mocksToVerify addObject:requestMock];
  return requestMock;
}

// Adds a peripheral service manager, and receives a read request accepted by the manager. The
// read request should be processed.
- (void)testDidReceiveReadRequest {
  id peripheralServiceManager = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager isAdvertising]).andReturn(YES);
  [self startWithPeripheralServiceManagers:@[ peripheralServiceManager ]];
  CBMutableCharacteristic *characteristicMock =
      [self prepareCharacteristicForPeripheralServiceManager:peripheralServiceManager];
  CBATTRequest *readRequestMock = [self prepareRequestForCharacteristic:characteristicMock];
  OCMStub([peripheralServiceManager canProcessReadRequest:readRequestMock])
      .andReturn(CBATTErrorSuccess);
  OCMExpect([peripheralServiceManager processReadRequest:readRequestMock]);
  OCMExpect(
      [_cbPeripheralManagerMock respondToRequest:readRequestMock withResult:CBATTErrorSuccess]);
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                  didReceiveReadRequest:readRequestMock];
}

// Adds a peripheral service manager, and receives a read request refused by the manager. The
// read request should not be processed.
- (void)testDidReceiveBadReadRequest {
  id peripheralServiceManager = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager isAdvertising]).andReturn(YES);
  [self startWithPeripheralServiceManagers:@[ peripheralServiceManager ]];
  CBMutableCharacteristic *characteristicMock =
      [self prepareCharacteristicForPeripheralServiceManager:peripheralServiceManager];
  CBATTRequest *readRequestMock = [self prepareRequestForCharacteristic:characteristicMock];
  OCMStub([peripheralServiceManager canProcessReadRequest:readRequestMock])
      .andReturn(CBATTErrorReadNotPermitted);
  OCMExpect([_cbPeripheralManagerMock respondToRequest:readRequestMock
                                            withResult:CBATTErrorReadNotPermitted]);
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                  didReceiveReadRequest:readRequestMock];
}

// Adds a peripheral service manager, and receives a read request to another service. The read
// request should not be processed.
- (void)testDidReceiveReadRequestToWrongService {
  id peripheralServiceManager = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager isAdvertising]).andReturn(YES);
  [self startWithPeripheralServiceManagers:@[ peripheralServiceManager ]];
  CBMutableCharacteristic *characteristicMock =
      [self prepareCharacteristicForCBService:nil withServiceUUID:nil];
  CBATTRequest *readRequestMock = [self prepareRequestForCharacteristic:characteristicMock];
  OCMExpect([_cbPeripheralManagerMock respondToRequest:readRequestMock
                                            withResult:CBATTErrorAttributeNotFound]);
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                  didReceiveReadRequest:readRequestMock];
}

// Adds a peripheral service manager, and receives a write request accepted by the manager. The
// write request should be processed.
- (void)testDidReceiveOneWriteRequest {
  id peripheralServiceManager = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager isAdvertising]).andReturn(YES);
  [self startWithPeripheralServiceManagers:@[ peripheralServiceManager ]];
  CBMutableCharacteristic *characteristicMock =
      [self prepareCharacteristicForPeripheralServiceManager:peripheralServiceManager];
  CBATTRequest *writeRequestMock = [self prepareRequestForCharacteristic:characteristicMock];
  OCMStub([peripheralServiceManager canProcessWriteRequest:writeRequestMock])
      .andReturn(CBATTErrorSuccess);
  OCMExpect([peripheralServiceManager processWriteRequest:writeRequestMock]);
  OCMExpect(
      [_cbPeripheralManagerMock respondToRequest:writeRequestMock withResult:CBATTErrorSuccess]);
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                didReceiveWriteRequests:@[ writeRequestMock ]];
}

// Adds a peripheral service manager, and receives a write request refused by the manager. The write
// request should not be processed.
- (void)testDidReceiveOneBadWriteRequest {
  id peripheralServiceManager = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager isAdvertising]).andReturn(YES);
  [self startWithPeripheralServiceManagers:@[ peripheralServiceManager ]];
  CBMutableCharacteristic *characteristicMock =
      [self prepareCharacteristicForPeripheralServiceManager:peripheralServiceManager];
  CBATTRequest *writeRequestMock = [self prepareRequestForCharacteristic:characteristicMock];
  OCMStub([peripheralServiceManager canProcessWriteRequest:writeRequestMock])
      .andReturn(CBATTErrorWriteNotPermitted);
  OCMExpect([_cbPeripheralManagerMock respondToRequest:writeRequestMock
                                            withResult:CBATTErrorWriteNotPermitted]);
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                didReceiveWriteRequests:@[ writeRequestMock ]];
}

// Adds a peripheral service manager, and receives a write request to another service. The write
// request should not be processed.
- (void)testDidReceiveOneWriteRequestToWrongService {
  id peripheralServiceManager = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager isAdvertising]).andReturn(YES);
  [self startWithPeripheralServiceManagers:@[ peripheralServiceManager ]];
  CBMutableCharacteristic *characteristicMock =
      [self prepareCharacteristicForCBService:nil withServiceUUID:nil];
  CBATTRequest *writeRequestMock = [self prepareRequestForCharacteristic:characteristicMock];
  OCMStub([_cbPeripheralManagerMock respondToRequest:writeRequestMock
                                          withResult:CBATTErrorAttributeNotFound]);
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                didReceiveWriteRequests:@[ writeRequestMock ]];
}

// Adds a peripheral service manager, and receives two write requests, only the second one is
// refused by the manager. None of those write requests should be processed. The error should be
// sent to the first write request.
- (void)testDidReceiveWriteRequestOneGoodAndOneBad {
  id peripheralServiceManager1 = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager1 isAdvertising]).andReturn(YES);
  id peripheralServiceManager2 = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager2 isAdvertising]).andReturn(YES);
  [self
      startWithPeripheralServiceManagers:@[ peripheralServiceManager1, peripheralServiceManager2 ]];
  // Good request
  CBMutableCharacteristic *characteristic1Mock =
      [self prepareCharacteristicForPeripheralServiceManager:peripheralServiceManager1];
  CBATTRequest *writeRequest1Mock = [self prepareRequestForCharacteristic:characteristic1Mock];
  OCMStub([peripheralServiceManager1 canProcessWriteRequest:writeRequest1Mock])
      .andReturn(CBATTErrorSuccess);
  // Bad request
  CBMutableCharacteristic *characteristic2Mock =
      [self prepareCharacteristicForPeripheralServiceManager:peripheralServiceManager2];
  CBATTRequest *writeRequest2Mock = [self prepareRequestForCharacteristic:characteristic2Mock];
  OCMStub([peripheralServiceManager2 canProcessWriteRequest:writeRequest2Mock])
      .andReturn(CBATTErrorWriteNotPermitted);
  OCMExpect([_cbPeripheralManagerMock respondToRequest:writeRequest1Mock
                                            withResult:CBATTErrorWriteNotPermitted]);
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                didReceiveWriteRequests:@[ writeRequest1Mock, writeRequest2Mock ]];
}

// Adds a peripheral service manager, and receives two write requests, only the first one is
// refused by the manager. None of those write requests should be processed. The error should be
// sent to the first write request.
- (void)testDidReceiveWriteRequestOneBadAndOneGood {
  id peripheralServiceManager1 = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager1 isAdvertising]).andReturn(YES);
  id peripheralServiceManager2 = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager2 isAdvertising]).andReturn(YES);
  [self
      startWithPeripheralServiceManagers:@[ peripheralServiceManager1, peripheralServiceManager2 ]];
  // Good request
  CBMutableCharacteristic *characteristic1Mock =
      [self prepareCharacteristicForPeripheralServiceManager:peripheralServiceManager1];
  CBATTRequest *writeRequest1Mock = [self prepareRequestForCharacteristic:characteristic1Mock];
  OCMStub([peripheralServiceManager1 canProcessWriteRequest:writeRequest1Mock])
      .andReturn(CBATTErrorWriteNotPermitted);
  // Bad request
  CBMutableCharacteristic *characteristic2Mock =
      [self prepareCharacteristicForPeripheralServiceManager:peripheralServiceManager2];
  CBATTRequest *writeRequest2Mock = [self prepareRequestForCharacteristic:characteristic2Mock];
  OCMStub([peripheralServiceManager2 canProcessWriteRequest:writeRequest2Mock])
      .andReturn(CBATTErrorSuccess);
  OCMExpect([_cbPeripheralManagerMock respondToRequest:writeRequest1Mock
                                            withResult:CBATTErrorWriteNotPermitted]);
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                didReceiveWriteRequests:@[ writeRequest1Mock, writeRequest2Mock ]];
}

// Adds a peripheral service manager, and receives two good write requests. Both write request
// should be processed. The success should be sent to the first write request.
- (void)testDidReceiveWriteRequestTwoGood {
  id peripheralServiceManager1 = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager1 isAdvertising]).andReturn(YES);
  id peripheralServiceManager2 = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager2 isAdvertising]).andReturn(YES);
  [self
      startWithPeripheralServiceManagers:@[ peripheralServiceManager1, peripheralServiceManager2 ]];
  // Good request
  CBMutableCharacteristic *characteristic1Mock =
      [self prepareCharacteristicForPeripheralServiceManager:peripheralServiceManager1];
  CBATTRequest *writeRequest1Mock = [self prepareRequestForCharacteristic:characteristic1Mock];
  OCMStub([peripheralServiceManager1 canProcessWriteRequest:writeRequest1Mock])
      .andReturn(CBATTErrorSuccess);
  OCMExpect([peripheralServiceManager1 processWriteRequest:writeRequest1Mock]);
  // Good request
  CBMutableCharacteristic *characteristic2Mock =
      [self prepareCharacteristicForPeripheralServiceManager:peripheralServiceManager2];
  CBATTRequest *writeRequest2Mock = [self prepareRequestForCharacteristic:characteristic2Mock];
  OCMStub([peripheralServiceManager2 canProcessWriteRequest:writeRequest2Mock])
      .andReturn(CBATTErrorSuccess);
  OCMExpect([peripheralServiceManager2 processWriteRequest:writeRequest2Mock]);
  OCMExpect(
      [_cbPeripheralManagerMock respondToRequest:writeRequest1Mock withResult:CBATTErrorSuccess]);
  [_peripheralManager peripheralManager:_cbPeripheralManagerMock
                didReceiveWriteRequests:@[ writeRequest1Mock, writeRequest2Mock ]];
}

- (GNSSocket *)createSocketMock {
  GNSSocket *socketMock = OCMStrictClassMock([GNSSocket class]);
  NSUUID *socketIdentifier = [NSUUID UUID];
  OCMStub([socketMock socketIdentifier]).andReturn(socketIdentifier);
  [_mocksToVerify addObject:socketMock];
  return socketMock;
}

// Adds one update value handler. Should be called right now.
- (void)testIsReadyToUpdateSubscribers {
  __block BOOL updateValueHandlerCalled = NO;
  GNSSocket *socketMock = [self createSocketMock];
  // Should be called right now.
  [_peripheralManager updateOutgoingCharOnSocket:socketMock
                                     withHandler:^() {
                                       updateValueHandlerCalled = YES;
                                       return GNSOutgoingCharUpdateNoReschedule;
                                     }];
  XCTAssertTrue(updateValueHandlerCalled);
}

// Adds one update value handler. The first one fails. Adds a second update value. It should not
// be called. The first one is set to not fail anymore. When the CB peripheral manager is ready
// to update subscribers, both handlers should be called.
- (void)testIsReadyToUpdateSubscriberBlocked {
  __block BOOL updateValueHandler1Called = NO;
  __block GNSOutgoingCharUpdate updateValueHandler1ReturnedValue =
      GNSOutgoingCharUpdateScheduleLater;
  GNSSocket *socketMock = [self createSocketMock];
  // Should be called right now.
  [_peripheralManager updateOutgoingCharOnSocket:socketMock
                                     withHandler:^() {
                                       updateValueHandler1Called = YES;
                                       return updateValueHandler1ReturnedValue;
                                     }];
  XCTAssertTrue(updateValueHandler1Called);
  __block BOOL updateValueHandler2Called = NO;
  __block GNSOutgoingCharUpdate updateValueHandler2ReturnedValue =
      GNSOutgoingCharUpdateNoReschedule;
  // Should no be called since the previous update value handler failed.
  [_peripheralManager updateOutgoingCharOnSocket:socketMock
                                     withHandler:^() {
                                       updateValueHandler2Called = YES;
                                       return updateValueHandler2ReturnedValue;
                                     }];
  XCTAssertFalse(updateValueHandler2Called);
  updateValueHandler1ReturnedValue = GNSOutgoingCharUpdateNoReschedule;
  updateValueHandler1Called = NO;
  // Should again again the first handler, and the second right after.
  [_peripheralManager peripheralManagerIsReadyToUpdateSubscribers:_cbPeripheralManagerMock];
  XCTAssertTrue(updateValueHandler1Called);
  XCTAssertTrue(updateValueHandler2Called);
}

// Adds 2 handlers on different sockets. One fails, the second one should process right away.
- (void)testTwoUpdateBlocksOnDifferentSocket {
  __block BOOL updateValueHandler1Called = NO;
  __block GNSOutgoingCharUpdate updateValueHandler1ReturnedValue =
      GNSOutgoingCharUpdateScheduleLater;
  GNSSocket *socketMock1 = [self createSocketMock];
  // Should be called right now.
  [_peripheralManager updateOutgoingCharOnSocket:socketMock1
                                     withHandler:^() {
                                       updateValueHandler1Called = YES;
                                       return updateValueHandler1ReturnedValue;
                                     }];
  XCTAssertTrue(updateValueHandler1Called);
  __block BOOL updateValueHandler2Called = NO;
  __block GNSOutgoingCharUpdate updateValueHandler2ReturnedValue =
      GNSOutgoingCharUpdateNoReschedule;
  GNSSocket *socketMock2 = [self createSocketMock];
  // Should no be called since the previous update value handler failed.
  [_peripheralManager updateOutgoingCharOnSocket:socketMock2
                                     withHandler:^() {
                                       updateValueHandler2Called = YES;
                                       return updateValueHandler2ReturnedValue;
                                     }];
  XCTAssertTrue(updateValueHandler2Called);
  updateValueHandler2Called = NO;
  updateValueHandler1ReturnedValue = GNSOutgoingCharUpdateNoReschedule;
  updateValueHandler1Called = NO;
  // Should again again the first handler, and the second one should not be called.
  [_peripheralManager peripheralManagerIsReadyToUpdateSubscribers:_cbPeripheralManagerMock];
  XCTAssertTrue(updateValueHandler1Called);
  XCTAssertFalse(updateValueHandler2Called);
}

- (void)testUpdateBlockCleanupAfterDisconnect {
  __block BOOL updateValueHandlerCalled = NO;
  GNSSocket *socketMock = [self createSocketMock];
  __block GNSOutgoingCharUpdate outgoingCharUpdateValue = GNSOutgoingCharUpdateScheduleLater;
  // Should be called right now.
  [_peripheralManager updateOutgoingCharOnSocket:socketMock
                                     withHandler:^() {
                                       updateValueHandlerCalled = YES;
                                       return outgoingCharUpdateValue;
                                     }];
  XCTAssertTrue(updateValueHandlerCalled);
  updateValueHandlerCalled = NO;
  outgoingCharUpdateValue = GNSOutgoingCharUpdateNoReschedule;
  OCMStub([socketMock isConnected]).andReturn(NO);
  [_peripheralManager socketDidDisconnect:socketMock];
  // Should again again the first handler, and the second right after.
  [_peripheralManager peripheralManagerIsReadyToUpdateSubscribers:_cbPeripheralManagerMock];
  XCTAssertTrue(updateValueHandlerCalled);
}

- (void)testUpdateValueForCentral {
  NSData *data = [NSData data];
  id characteristicMock = OCMStrictClassMock([CBMutableCharacteristic class]);
  id centralMock = OCMStrictClassMock([CBCentral class]);
  OCMStub([_cbPeripheralManagerMock updateValue:data
                              forCharacteristic:characteristicMock
                           onSubscribedCentrals:@[ centralMock ]]);
  GNSPeripheralServiceManager *peripheralServiceManager =
      OCMStrictClassMock([GNSPeripheralServiceManager class]);
  OCMStub([peripheralServiceManager weaveOutgoingCharacteristic]).andReturn(characteristicMock);
  GNSSocket *socketMock = OCMStrictClassMock([GNSSocket class]);
  OCMStub([socketMock owner]).andReturn(peripheralServiceManager);
  OCMStub([socketMock peerAsCentral]).andReturn(centralMock);
  [_peripheralManager updateOutgoingCharacteristic:data onSocket:socketMock];
  OCMVerifyAll(characteristicMock);
  OCMVerifyAll(centralMock);
}

- (void)testRecoverFromBTCrashLoop {
  GNSPeripheralServiceManager *peripheralServiceManager = [self peripheralServiceManager];
  OCMStub([peripheralServiceManager isAdvertising]).andReturn(YES);
  [self startWithPeripheralServiceManagers:@[ peripheralServiceManager ]];
  for (int i = 0; i < 5; ++i) {
    OCMExpect([_cbPeripheralManagerMock removeAllServices]);
    [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateResetting];
    CBMutableService *service = [peripheralServiceManager cbService];
    OCMExpect([_cbPeripheralManagerMock addService:service]);
    [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOff];
    XCTAssertEqual(GNSBluetoothServiceStateAddInProgress,
                   [peripheralServiceManager cbServiceState]);
  }

  // After 5 consecutive resetting state, the services are no longer added by the peripheral
  // manager.
  OCMExpect([_cbPeripheralManagerMock removeAllServices]);
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStateResetting];
  [self updatePeripheralManagerWithPeripheralManagerState:CBPeripheralManagerStatePoweredOff];
  XCTAssertEqual(GNSBluetoothServiceStateNotAdded, [peripheralServiceManager cbServiceState]);
}

@end
