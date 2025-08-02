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

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Central/GNSCentralManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Central/GNSCentralPeerManager+Private.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@interface TestGNSCentralManager : GNSCentralManager
@end

@implementation TestGNSCentralManager

+ (CBCentralManager *)centralManagerWithDelegate:(id<CBCentralManagerDelegate>)delegate
                                           queue:(dispatch_queue_t)queue
                                         options:(NSDictionary<NSString *, id> *)options {
  CBCentralManager *mockManager = OCMStrictClassMock([CBCentralManager class]);
  OCMStub([mockManager delegate]).andReturn(delegate);
  return mockManager;
}

- (GNSCentralPeerManager *)createCentralPeerManagerWithPeripheral:(CBPeripheral *)peripheral {
  GNSCentralPeerManager *mockPeerManager = OCMStrictClassMock([GNSCentralPeerManager class]);
  OCMStub([mockPeerManager cbPeripheral]).andReturn(peripheral);
  return mockPeerManager;
}

@end

@interface GNSCentralManagerTest : XCTestCase
@property(nonatomic) CBUUID *socketServiceUUID;
@property(nonatomic) TestGNSCentralManager *centralManager;
@property(nonatomic) CBCentralManager *cbCentralManagerMock;
@property(nonatomic) CBManagerState cbCentralManagerState;
@property(nonatomic) NSMutableArray<OCMockObject *> *mockObjectsToVerify;
@property(nonatomic) id<GNSCentralManagerDelegate> centralManagerDelegate;
@end

@implementation GNSCentralManagerTest

- (void)setUp {
  [super setUp];
  self.mockObjectsToVerify = [NSMutableArray array];
  self.socketServiceUUID = [CBUUID UUIDWithNSUUID:[NSUUID UUID]];
  self.centralManager =
      [[TestGNSCentralManager alloc] initWithSocketServiceUUID:self.socketServiceUUID
                                                         queue:dispatch_get_main_queue()];

  self.centralManagerDelegate = OCMStrictProtocolMock(@protocol(GNSCentralManagerDelegate));
  self.centralManager.delegate = self.centralManagerDelegate;

  XCTAssertFalse(self.centralManager.scanning);
  XCTAssertEqualObjects(self.centralManager.socketServiceUUID, self.socketServiceUUID);

  self.cbCentralManagerMock = self.centralManager.testing_cbCentralManager;
  XCTAssertEqual(self.cbCentralManagerMock.delegate, self.centralManager);
  self.cbCentralManagerState = CBManagerStatePoweredOff;
  OCMStub([self.cbCentralManagerMock state]).andDo(^(NSInvocation *invocation) {
    CBManagerState state = self.cbCentralManagerState;
    [invocation setReturnValue:&state];
  });
}

- (void)tearDown {
  OCMVerifyAll(self.cbCentralManagerMock);
  OCMVerifyAll(self.centralManagerDelegate);
  for (OCMockObject *object in self.mockObjectsToVerify) {
    OCMVerifyAll(object);
  }
  [super tearDown];
}

#pragma mark - Scanning And Power Off/On Bluetooth

- (void)startScanningWithServices:(NSArray<CBUUID *> *)services
                advertisementName:(NSString *)advertisementName {
  NSDictionary<NSString *, id> *options = @{CBCentralManagerScanOptionAllowDuplicatesKey : @YES};
  OCMExpect([self.cbCentralManagerMock scanForPeripheralsWithServices:services options:options]);
  [self.centralManager startScanWithAdvertisedName:advertisementName
                             advertisedServiceUUID:self.centralManager.socketServiceUUID];
  XCTAssertTrue(self.centralManager.scanning);
}

- (void)testScanningWithBluetoothOFF {
  [self.centralManager startScanWithAdvertisedName:nil
                             advertisedServiceUUID:self.centralManager.socketServiceUUID];
  XCTAssertTrue(self.centralManager.scanning);
  [self.centralManager stopScan];
  XCTAssertFalse(self.centralManager.scanning);
}

- (void)testScanningWithBluetoothON {
  self.cbCentralManagerState = CBManagerStatePoweredOn;
  NSArray<CBUUID *> *services = @[ self.socketServiceUUID ];
  [self startScanningWithServices:services advertisementName:nil];
  OCMExpect([self.cbCentralManagerMock stopScan]);
  [self.centralManager stopScan];
  XCTAssertFalse(self.centralManager.scanning);
}

- (void)testScanningAndPowerONBluetooth {
  // Start scanning while powered off.
  [self.centralManager startScanWithAdvertisedName:nil
                             advertisedServiceUUID:self.centralManager.socketServiceUUID];

  // Expect a state update and scan command when power turns on.
  OCMExpect([self.centralManagerDelegate centralManagerDidUpdateBleState:self.centralManager]);
  OCMExpect([self.cbCentralManagerMock
      scanForPeripheralsWithServices:@[ self.socketServiceUUID ]
                             options:@{CBCentralManagerScanOptionAllowDuplicatesKey : @YES}]);

  self.cbCentralManagerState = CBManagerStatePoweredOn;
  [self.centralManager centralManagerDidUpdateState:self.cbCentralManagerMock];

  XCTAssertTrue(self.centralManager.scanning);
  OCMExpect([self.cbCentralManagerMock stopScan]);
  [self.centralManager stopScan];
  XCTAssertFalse(self.centralManager.scanning);
}

- (void)testScanningAndPowerOFFBluetooth {
  self.cbCentralManagerState = CBManagerStatePoweredOn;
  NSArray<CBUUID *> *services = @[ self.socketServiceUUID ];
  [self startScanningWithServices:services advertisementName:nil];

  // Expect a state update when power turns off. The stopScan is implicitly called.
  OCMExpect([self.centralManagerDelegate centralManagerDidUpdateBleState:self.centralManager]);
  OCMExpect([self.cbCentralManagerMock stopScan]);

  self.cbCentralManagerState = CBManagerStatePoweredOff;
  [self.centralManager centralManagerDidUpdateState:self.cbCentralManagerMock];

  XCTAssertTrue(self.centralManager.scanning);
  [self.centralManager stopScan];
  XCTAssertFalse(self.centralManager.scanning);
}

- (void)testScanningWithAdvertisedName {
  self.cbCentralManagerState = CBManagerStatePoweredOn;
  [self startScanningWithServices:nil advertisementName:@"advertisedname"];
  OCMExpect([self.cbCentralManagerMock stopScan]);
  [self.centralManager stopScan];
  XCTAssertFalse(self.centralManager.scanning);
}

#pragma mark - Scanning with no advertised name

- (void)testScanningPeripheralWithWrongPeripheralService {
  self.cbCentralManagerState = CBManagerStatePoweredOn;
  [self startScanningWithServices:@[ self.socketServiceUUID ] advertisementName:nil];
  CBPeripheral *peripheral = OCMStrictClassMock([CBPeripheral class]);
  NSDictionary<NSString *, id> *advertisementData =
      @{CBAdvertisementDataServiceUUIDsKey : @[ [NSUUID UUID] ]};

  // Expect no delegate callback since the service UUID does not match.
  [self.centralManager centralManager:self.cbCentralManagerMock
                didDiscoverPeripheral:peripheral
                    advertisementData:advertisementData
                                 RSSI:@(19)];
  OCMVerifyAll(peripheral);
}

- (void)testScanningPeripheral {
  self.cbCentralManagerState = CBManagerStatePoweredOn;
  [self startScanningWithServices:@[ self.socketServiceUUID ] advertisementName:nil];
  CBPeripheral *peripheral = OCMStrictClassMock([CBPeripheral class]);
  OCMStub([peripheral identifier]).andReturn([NSUUID UUID]);
  NSDictionary<NSString *, id> *advertisementData =
      @{CBAdvertisementDataServiceUUIDsKey : @[ self.socketServiceUUID ]};
  OCMExpect([self.centralManagerDelegate centralManager:self.centralManager
                                        didDiscoverPeer:[OCMArg any]
                                      advertisementData:[OCMArg any]])
      .andDo(^(NSInvocation *invocation) {
        __unsafe_unretained GNSCentralPeerManager *centralPeerManager;
        [invocation getArgument:&centralPeerManager atIndex:3];
        XCTAssertEqual(centralPeerManager.cbPeripheral, peripheral);
      });

  [self.centralManager centralManager:self.cbCentralManagerMock
                didDiscoverPeripheral:peripheral
                    advertisementData:advertisementData
                                 RSSI:@(42)];
  OCMVerifyAll(peripheral);
}

#pragma mark - Scanning with advertised name

- (void)testScanningPeripheralWithWrongAdvertisedName {
  _cbCentralManagerState = CBManagerStatePoweredOn;
  [self startScanningWithServices:nil advertisementName:@"advertisedname"];
  CBPeripheral *peripheral = OCMStrictClassMock([CBPeripheral class]);
  NSDictionary<NSString *, id> *advertisementData = @{
    CBAdvertisementDataServiceUUIDsKey : @[ [NSUUID UUID] ],
    CBAdvertisementDataLocalNameKey : @"wrongadvertisedname",
  };

  // The central manager's delegate is not expected to be called because the
  // advertised name "wrongadvertisedname" does not match "advertisedname".
  // The strict mock on the delegate will ensure this behavior is verified.
  [self.centralManager centralManager:self.cbCentralManagerMock
                didDiscoverPeripheral:peripheral
                    advertisementData:advertisementData
                                 RSSI:@(19)];
}

- (void)testScanningPeripheralWithRightAdvertisedName {
  _cbCentralManagerState = CBManagerStatePoweredOn;
  NSString *advertisedName = @"advertisedname";
  [self startScanningWithServices:nil advertisementName:advertisedName];

  CBPeripheral *peripheral = OCMStrictClassMock([CBPeripheral class]);
  OCMStub([peripheral identifier]).andReturn([NSUUID UUID]);

  NSDictionary<NSString *, id> *advertisementData = @{
    CBAdvertisementDataServiceUUIDsKey : @[ [NSUUID UUID] ],
    CBAdvertisementDataLocalNameKey : advertisedName,
  };

  // Expect the delegate to be called because the advertised name matches.
  // Use a checkWithBlock to verify the correct peripheral is passed.
  id peerManagerCheck = [OCMArg checkWithBlock:^BOOL(id obj) {
    GNSCentralPeerManager *peer = (GNSCentralPeerManager *)obj;
    XCTAssertEqual(peer.cbPeripheral, peripheral);
    return YES;
  }];
  OCMExpect([self.centralManagerDelegate centralManager:self.centralManager
                                        didDiscoverPeer:peerManagerCheck
                                      advertisementData:advertisementData]);

  [self.centralManager centralManager:self.cbCentralManagerMock
                didDiscoverPeripheral:peripheral
                    advertisementData:advertisementData
                                 RSSI:@(19)];
}

#pragma mark - Peripheral Retrieval

- (CBPeripheral *)prepareCBPeripheralWithIdentifier:(NSUUID *)identifier
                                    peripheralState:(CBPeripheralState *)peripheralState {
  CBPeripheral *peripheral = OCMStrictClassMock([CBPeripheral class]);
  OCMStub([peripheral identifier]).andReturn(identifier);
  OCMStub([peripheral state]).andDo(^(NSInvocation *invocation) {
    [invocation setReturnValue:peripheralState];
  });
  [self.mockObjectsToVerify addObject:(OCMockObject *)peripheral];
  return peripheral;
}

- (GNSCentralPeerManager *)retrieveCentralPeerWithPeripheral:(CBPeripheral *)peripheral {
  NSUUID *identifier = [peripheral identifier];
  OCMExpect([self.cbCentralManagerMock retrievePeripheralsWithIdentifiers:@[ identifier ]])
      .andReturn(@[ peripheral ]);
  GNSCentralPeerManager *centralPeerManager =
      [self.centralManager retrieveCentralPeerWithIdentifier:identifier];
  OCMStub([centralPeerManager identifier]).andReturn(identifier);
  XCTAssertNotNil(centralPeerManager);
  [self.mockObjectsToVerify addObject:(OCMockObject *)centralPeerManager];
  return centralPeerManager;
}

- (void)testRetrieveCentralPeerAndConnectDisconnect {
  self.cbCentralManagerState = CBManagerStatePoweredOn;
  NSUUID *identifier = [NSUUID UUID];
  CBPeripheralState peripheralMockState = CBPeripheralStateConnected;
  CBPeripheral *peripheral = [self prepareCBPeripheralWithIdentifier:identifier
                                                     peripheralState:&peripheralMockState];
  GNSCentralPeerManager *centralPeerManager = [self retrieveCentralPeerWithPeripheral:peripheral];
  OCMExpect([centralPeerManager bleConnected]);
  [self.centralManager centralManager:self.cbCentralManagerMock didConnectPeripheral:peripheral];
  NSError *error = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMExpect([centralPeerManager bleDisconnectedWithError:error]);
  peripheralMockState = CBPeripheralStateDisconnected;
  [self.centralManager centralManager:self.cbCentralManagerMock
              didDisconnectPeripheral:peripheral
                                error:error];
}

- (void)testRetrieveTwiceCentralPeer {
  self.cbCentralManagerState = CBManagerStatePoweredOn;
  NSUUID *identifier = [NSUUID UUID];
  CBPeripheralState peripheralMockState = CBPeripheralStateConnected;
  CBPeripheral *peripheral = [self prepareCBPeripheralWithIdentifier:identifier
                                                     peripheralState:&peripheralMockState];
  GNSCentralPeerManager *centralPeerManager = [self retrieveCentralPeerWithPeripheral:peripheral];
  XCTAssertNil([self.centralManager retrieveCentralPeerWithIdentifier:identifier]);
  OCMExpect([centralPeerManager bleConnected]);
  [self.centralManager centralManager:self.cbCentralManagerMock didConnectPeripheral:peripheral];
  NSError *error = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMExpect([centralPeerManager bleDisconnectedWithError:error]);
  peripheralMockState = CBPeripheralStateDisconnected;
  [self.centralManager centralManager:self.cbCentralManagerMock
              didDisconnectPeripheral:peripheral
                                error:error];
  [self.centralManager centralPeerManagerDidDisconnect:centralPeerManager];

  // Retrieve a second, different peer.
  NSUUID *identifier2 = [NSUUID UUID];
  CBPeripheralState peripheralState2 = CBPeripheralStateConnected;
  CBPeripheral *peripheral2 = [self prepareCBPeripheralWithIdentifier:identifier2
                                                      peripheralState:&peripheralState2];
  GNSCentralPeerManager *centralPeerManager2 = [self retrieveCentralPeerWithPeripheral:peripheral2];
  XCTAssertNotNil(centralPeerManager2);
  XCTAssertNotEqual(centralPeerManager, centralPeerManager2);
}

- (void)testRetrieveCentralPeerAndFailToConnect {
  self.cbCentralManagerState = CBManagerStatePoweredOn;
  NSUUID *identifier = [NSUUID UUID];
  CBPeripheralState peripheralMockState = CBPeripheralStateConnected;
  CBPeripheral *peripheral = [self prepareCBPeripheralWithIdentifier:identifier
                                                     peripheralState:&peripheralMockState];
  GNSCentralPeerManager *centralPeerManager = [self retrieveCentralPeerWithPeripheral:peripheral];
  OCMExpect([centralPeerManager bleConnected]);
  [self.centralManager centralManager:self.cbCentralManagerMock didConnectPeripheral:peripheral];

  NSError *error = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMExpect([centralPeerManager bleDisconnectedWithError:error]);
  peripheralMockState = CBPeripheralStateDisconnected;
  [self.centralManager centralManager:self.cbCentralManagerMock
           didFailToConnectPeripheral:peripheral
                                error:error];
}

@end