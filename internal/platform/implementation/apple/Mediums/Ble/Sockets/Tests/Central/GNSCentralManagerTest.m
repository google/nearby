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
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@interface TestGNSCentralManager : GNSCentralManager
@end

@implementation TestGNSCentralManager

+ (CBCentralManager *)centralManagerWithDelegate:(id<CBCentralManagerDelegate>)delegate
                                           queue:(dispatch_queue_t)queue
                                         options:(NSDictionary *)options {
  CBCentralManager *result = OCMStrictClassMock([CBCentralManager class]);
  OCMStub([result delegate]).andReturn(delegate);
  return result;
}

- (GNSCentralPeerManager *)createCentralPeerManagerWithPeripheral:(CBPeripheral *)peripheral {
  GNSCentralPeerManager *manager = OCMStrictClassMock([GNSCentralPeerManager class]);
  OCMStub([manager cbPeripheral]).andReturn(peripheral);
  return manager;
}

@end

@interface GNSCentralManagerTest : XCTestCase {
  CBUUID *_socketServiceUUID;
  TestGNSCentralManager *_centralManager;
  CBCentralManager *_cbCentralManagerMock;
  CBCentralManagerState _cbCentralManagerState;
  NSMutableArray *_mockObjectsToVerify;
  id<GNSCentralManagerDelegate> _centralManagerDelegate;
}
@end

@implementation GNSCentralManagerTest

- (void)setUp {
  _mockObjectsToVerify = [NSMutableArray array];
  _socketServiceUUID = [CBUUID UUIDWithNSUUID:[NSUUID UUID]];
  _centralManager = [[TestGNSCentralManager alloc]
      initWithSocketServiceUUID:_socketServiceUUID
                          queue:dispatch_get_main_queue()];
  _centralManagerDelegate = OCMStrictProtocolMock(@protocol(GNSCentralManagerDelegate));
  _centralManager.delegate = _centralManagerDelegate;
  XCTAssertFalse(_centralManager.scanning);
  XCTAssertEqualObjects(_centralManager.socketServiceUUID, _socketServiceUUID);
  _cbCentralManagerMock = _centralManager.testing_cbCentralManager;
  XCTAssertEqual(_cbCentralManagerMock.delegate, _centralManager);
  _cbCentralManagerState = CBCentralManagerStatePoweredOff;
  OCMStub([_cbCentralManagerMock state])
      .andDo(^(NSInvocation *invocation) {
        [invocation setReturnValue:&_cbCentralManagerState];
      });
}

- (void)tearDown {
  OCMVerifyAll((id)_cbCentralManagerMock);
  OCMVerifyAll((id)_centralManagerDelegate);
  for (OCMockObject *object in _mockObjectsToVerify) {
    OCMVerifyAll(object);
  }
}

#pragma mark - Scanning And Power Off/On Bluetooth

- (void)startScanningWithServices:(NSArray *)services
                 advertismentName:(NSString *)advertismentName {
  NSDictionary *options = @{ CBCentralManagerScanOptionAllowDuplicatesKey : @YES };
  OCMExpect([_cbCentralManagerMock scanForPeripheralsWithServices:services options:options]);
  [_centralManager startScanWithAdvertisedName:advertismentName
                         advertisedServiceUUID:_centralManager.socketServiceUUID];
  XCTAssertTrue(_centralManager.scanning);
}

- (void)testScanningWithBluetoothOFF {
  [_centralManager startScanWithAdvertisedName:nil
                         advertisedServiceUUID:_centralManager.socketServiceUUID];
  XCTAssertTrue(_centralManager.scanning);
  [_centralManager stopScan];
  XCTAssertFalse(_centralManager.scanning);
}

- (void)testScanningWithBluetoothON {
  _cbCentralManagerState = CBCentralManagerStatePoweredOn;
  NSArray *services = @[ _socketServiceUUID ];
  [self startScanningWithServices:services advertismentName:nil];
  OCMExpect([_cbCentralManagerMock stopScan]);
  [_centralManager stopScan];
  XCTAssertFalse(_centralManager.scanning);
}

- (void)testScanningAndPowerONBluetooth {
  NSArray *services = @[ _socketServiceUUID ];
  [self startScanningWithServices:services advertismentName:nil];
  OCMExpect([_centralManagerDelegate centralManagerDidUpdateBLEState:_centralManager]);
  _cbCentralManagerState = CBCentralManagerStatePoweredOn;
  [_centralManager centralManagerDidUpdateState:_cbCentralManagerMock];
  XCTAssertTrue(_centralManager.scanning);
  OCMExpect([_cbCentralManagerMock stopScan]);
  [_centralManager stopScan];
  XCTAssertFalse(_centralManager.scanning);
}

- (void)testScanningAndPowerOFFBluetooth {
  _cbCentralManagerState = CBCentralManagerStatePoweredOn;
  NSArray *services = @[ _socketServiceUUID ];
  [self startScanningWithServices:services advertismentName:nil];
  OCMExpect([_centralManagerDelegate centralManagerDidUpdateBLEState:_centralManager]);
  OCMExpect([_cbCentralManagerMock stopScan]);
  _cbCentralManagerState = CBCentralManagerStatePoweredOff;
  [_centralManager centralManagerDidUpdateState:_cbCentralManagerMock];
  XCTAssertTrue(_centralManager.scanning);
  [_centralManager stopScan];
  XCTAssertFalse(_centralManager.scanning);
}

- (void)testScanningWithAdvertisedName {
  _cbCentralManagerState = CBCentralManagerStatePoweredOn;
  [self startScanningWithServices:nil advertismentName:@"advertisedname"];
  OCMExpect([_cbCentralManagerMock stopScan]);
  [_centralManager stopScan];
  XCTAssertFalse(_centralManager.scanning);
}

#pragma mark - Scanning with no advertised name

- (void)testScanningPeripheralWithWrongPeripheralService {
  _cbCentralManagerState = CBCentralManagerStatePoweredOn;
  [self startScanningWithServices:@[ _socketServiceUUID ] advertismentName:nil];
  CBPeripheral *peripheral = OCMStrictClassMock([CBPeripheral class]);
  NSDictionary *advertisementData = @{ CBAdvertisementDataServiceUUIDsKey: @[ [NSUUID UUID] ] };
  [_centralManager centralManager:_cbCentralManagerMock
            didDiscoverPeripheral:peripheral
                advertisementData:advertisementData
                             RSSI:@(19)];
}

- (void)testScanningPeripheral {
  _cbCentralManagerState = CBCentralManagerStatePoweredOn;
  [self startScanningWithServices:@[ _socketServiceUUID ] advertismentName:nil];
  CBPeripheral *peripheral = OCMStrictClassMock([CBPeripheral class]);
  OCMStub([peripheral identifier]).andReturn([NSUUID UUID]);
  NSDictionary *advertisementData = @{
    CBAdvertisementDataServiceUUIDsKey : @[ _socketServiceUUID ]
  };
  OCMExpect([_centralManagerDelegate
         centralManager:_centralManager
        didDiscoverPeer:[OCMArg checkWithBlock:^BOOL(GNSCentralPeerManager *centralPeerManager) {
        XCTAssertEqual(centralPeerManager.cbPeripheral, peripheral);
        return YES;
      }]
      advertisementData:[OCMArg any]]);
  [_centralManager centralManager:_cbCentralManagerMock
            didDiscoverPeripheral:peripheral
                advertisementData:advertisementData
                             RSSI:@(42)];
}

#pragma mark - Scanning with advertised name

- (void)testScanningPeripheralWithWrongAdvertisedName {
  _cbCentralManagerState = CBCentralManagerStatePoweredOn;
  [self startScanningWithServices:nil advertismentName:@"advertisedname"];
  CBPeripheral *peripheral = OCMStrictClassMock([CBPeripheral class]);
  NSDictionary *advertisementData = @{
    CBAdvertisementDataServiceUUIDsKey : @[ [NSUUID UUID] ],
    CBAdvertisementDataLocalNameKey : @"wrongadvertisedname",
  };
  [_centralManager centralManager:_cbCentralManagerMock
            didDiscoverPeripheral:peripheral
                advertisementData:advertisementData
                             RSSI:@(19)];
}

- (void)testScanningPeripheralWithRightAdvertisedName {
  _cbCentralManagerState = CBCentralManagerStatePoweredOn;
  NSString *advertisedName = @"advertisedname";
  [self startScanningWithServices:nil advertismentName:advertisedName];
  CBPeripheral *peripheral = OCMStrictClassMock([CBPeripheral class]);
  OCMStub([peripheral identifier]).andReturn([NSUUID UUID]);
  NSDictionary *advertisementData = @{
    CBAdvertisementDataServiceUUIDsKey : @[ _socketServiceUUID ],
    CBAdvertisementDataLocalNameKey : advertisedName,
  };
  OCMExpect([_centralManagerDelegate centralManager:_centralManager
                                    didDiscoverPeer:[OCMArg any]
                                  advertisementData:[OCMArg any]]);
  [_centralManager centralManager:_cbCentralManagerMock
            didDiscoverPeripheral:peripheral
                advertisementData:advertisementData
                             RSSI:@(19)];
}

#pragma mark - Peripheral Retrieval

- (CBPeripheral *)prepareCBPeripheralWithIdentifier:(NSUUID *)identifier
                                    peripheralState:(CBPeripheralState *)peripheralState {
  CBPeripheral *peripheral = OCMStrictClassMock([CBPeripheral class]);
  OCMStub([peripheral identifier]).andReturn(identifier);
  OCMStub([peripheral state])
      .andDo(^(NSInvocation *invocation) {
        [invocation setReturnValue:peripheralState];
      });
  [_mockObjectsToVerify addObject:(OCMockObject *)peripheral];
  return peripheral;
}

- (GNSCentralPeerManager *)retrieveCentralPeerWithPeripheral:(CBPeripheral *)peripheral {
  NSUUID *identifier = peripheral.identifier;
  OCMExpect([_cbCentralManagerMock retrievePeripheralsWithIdentifiers:@[ identifier ]])
      .andReturn([NSArray arrayWithObject:peripheral]);
  GNSCentralPeerManager *centralPeerManager =
      [_centralManager retrieveCentralPeerWithIdentifier:identifier];
  OCMStub([centralPeerManager cbPeripheral]).andReturn(peripheral);
  OCMStub([centralPeerManager identifier]).andReturn(identifier);
  XCTAssertNotNil(centralPeerManager);
  [_mockObjectsToVerify addObject:(OCMockObject *)centralPeerManager];
  return centralPeerManager;
}

- (void)testRetrieveCentralPeerAndConnectDisconnect {
  _cbCentralManagerState = CBCentralManagerStatePoweredOn;
  NSUUID *identifier = [NSUUID UUID];
  CBPeripheralState peripheralMockState = CBPeripheralStateConnected;
  CBPeripheral *peripheral =
      [self prepareCBPeripheralWithIdentifier:identifier peripheralState:&peripheralMockState];
  GNSCentralPeerManager *centralPeerManager = [self retrieveCentralPeerWithPeripheral:peripheral];
  OCMExpect([centralPeerManager bleConnected]);
  [_centralManager centralManager:_cbCentralManagerMock didConnectPeripheral:peripheral];
  NSError *error = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMExpect([centralPeerManager bleDisconnectedWithError:error]);
  peripheralMockState = CBPeripheralStateDisconnected;
  [_centralManager centralManager:_cbCentralManagerMock
          didDisconnectPeripheral:peripheral
                            error:error];
}

- (void)testRetrieveTwiceCentralPeer {
  _cbCentralManagerState = CBCentralManagerStatePoweredOn;
  NSUUID *identifier = [NSUUID UUID];
  CBPeripheralState peripheralMockState = CBPeripheralStateConnected;
  CBPeripheral *peripheral =
      [self prepareCBPeripheralWithIdentifier:identifier peripheralState:&peripheralMockState];
  GNSCentralPeerManager *centralPeerManager = [self retrieveCentralPeerWithPeripheral:peripheral];
  XCTAssertNil([_centralManager retrieveCentralPeerWithIdentifier:identifier]);
  OCMExpect([centralPeerManager bleConnected]);
  [_centralManager centralManager:_cbCentralManagerMock didConnectPeripheral:peripheral];
  NSError *error = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMExpect([centralPeerManager bleDisconnectedWithError:error]);
  peripheralMockState = CBPeripheralStateDisconnected;
  [_centralManager centralManager:_cbCentralManagerMock
          didDisconnectPeripheral:peripheral
                            error:error];
  [_centralManager centralPeerManagerDidDisconnect:centralPeerManager];

  NSUUID *identifier2 = [NSUUID UUID];
  CBPeripheralState peripheralState2 = CBPeripheralStateConnected;
  CBPeripheral *peripheral2 =
      [self prepareCBPeripheralWithIdentifier:identifier2 peripheralState:&peripheralState2];
  GNSCentralPeerManager *centralPeerManager2 = [self retrieveCentralPeerWithPeripheral:peripheral2];
  XCTAssertNotNil(centralPeerManager2);
  XCTAssertNotEqual(centralPeerManager, centralPeerManager2);
}

- (void)testRetrieveCentralPeerAndConnectFailToConnect {
  _cbCentralManagerState = CBCentralManagerStatePoweredOn;
  NSUUID *identifier = [NSUUID UUID];
  CBPeripheralState peripheralMockState = CBPeripheralStateConnected;
  CBPeripheral *peripheral =
      [self prepareCBPeripheralWithIdentifier:identifier peripheralState:&peripheralMockState];
  GNSCentralPeerManager *centralPeerManager = [self retrieveCentralPeerWithPeripheral:peripheral];
  OCMExpect([centralPeerManager bleConnected]);
  [_centralManager centralManager:_cbCentralManagerMock didConnectPeripheral:peripheral];
  NSError *error = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMExpect([centralPeerManager bleDisconnectedWithError:error]);
  peripheralMockState = CBPeripheralStateDisconnected;
  [_centralManager centralManager:_cbCentralManagerMock
       didFailToConnectPeripheral:peripheral
                            error:error];
}

@end
