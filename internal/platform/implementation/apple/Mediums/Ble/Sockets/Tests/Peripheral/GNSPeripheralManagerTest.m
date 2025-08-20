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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralManager.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralServiceManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralServiceManager.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Tests/Peripheral/GNSFakePeripheralManager.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@interface GNSPeripheralManagerTest : XCTestCase
@end

@implementation GNSPeripheralManagerTest {
}

- (void)setUp {
  [super setUp];
}

- (void)tearDown {
  [super tearDown];
}

// Private helper to set up common mocks and manager for each test.
- (NSDictionary *)setupCommonMocksAndManager {
  __block NSMutableDictionary *mutableMocks = [@{} mutableCopy];
  id mockCBPeripheralManager = OCMClassMock([CBPeripheralManager class]);

  OCMExpect([mockCBPeripheralManager setDelegate:[OCMArg any]]);

  [mutableMocks setObject:@(CBManagerStateUnknown) forKey:@"simulatedPeripheralManagerState"];
  OCMStub([mockCBPeripheralManager state]).andDo(^(NSInvocation *invocation) {
    CBManagerState currentState = [mutableMocks[@"simulatedPeripheralManagerState"] intValue];
    [invocation setReturnValue:&currentState];
  });

  [mutableMocks setObject:@(NO) forKey:@"simulatedIsAdvertising"];
  OCMStub([mockCBPeripheralManager isAdvertising]).andDo(^(NSInvocation *invocation) {
    BOOL currentIsAdvertising = [mutableMocks[@"simulatedIsAdvertising"] boolValue];
    [invocation setReturnValue:&currentIsAdvertising];
  });

  GNSFakePeripheralManager *fakePeripheralManager =
      [[GNSFakePeripheralManager alloc] initWithAdvertisedName:@"Test"
                                             restoreIdentifier:nil
                                                         queue:dispatch_get_main_queue()
                                             peripheralManager:mockCBPeripheralManager];

  [mutableMocks setObject:fakePeripheralManager forKey:@"fakePeripheralManager"];
  [mutableMocks setObject:mockCBPeripheralManager forKey:@"mockCBPeripheralManager"];

  return [mutableMocks copy];
}

- (void)testSocketDidDisconnect {
  NSDictionary *mocks = [self setupCommonMocksAndManager];
  id mockCBPeripheralManager = mocks[@"mockCBPeripheralManager"];

  // Create a real GNSPeripheralManager instance for this test
  GNSPeripheralManager *peripheralManager =
      [[GNSPeripheralManager alloc] initWithAdvertisedName:@"Test"
                                         restoreIdentifier:nil
                                                     queue:dispatch_get_main_queue()
                                         peripheralManager:mockCBPeripheralManager];

  // Mock a GNSSocket
  id mockSocket = OCMClassMock([GNSSocket class]);
  NSUUID *socketUUID = [NSUUID UUID];
  OCMStub([mockSocket socketIdentifier]).andReturn(socketUUID);
  OCMStub([mockSocket isConnected]).andReturn(NO);
  // Access the private ivar to inject a handler queue
  NSMutableDictionary *handlerQueuePerSocketIdentifier =
      [peripheralManager valueForKey:@"_handlerQueuePerSocketIdentifier"];

  // Add a dummy handler queue for the mock socket
  NSMutableArray *handlerQueue = [NSMutableArray array];
  __block BOOL handlerCalled = NO;
  GNSUpdateValueHandler dummyHandler = ^BOOL() {
    handlerCalled = YES;
    return YES;
  };
  [handlerQueue addObject:dummyHandler];
  handlerQueuePerSocketIdentifier[socketUUID] = handlerQueue;

  XCTAssertNotNil(handlerQueuePerSocketIdentifier[socketUUID]);

  // Call the method to be tested
  [peripheralManager socketDidDisconnect:mockSocket];

  // Verify the handler was called and the queue is removed
  XCTAssertTrue(handlerCalled);
  XCTAssertNil(handlerQueuePerSocketIdentifier[socketUUID]);
}

// Tests that the peripheral manager is initialized with the correct properties.
/*
- (void)testInitialization {
  NSDictionary *mocks = [self setupCommonMocksAndManager];
  GNSFakePeripheralManager *fakePeripheralManager = mocks[@"fakePeripheralManager"];
  id mockCBPeripheralManager = mocks[@"mockCBPeripheralManager"];

  XCTAssertEqualObjects(fakePeripheralManager.advertisedName, @"Test");
  XCTAssertNil(fakePeripheralManager.restoreIdentifier);
  XCTAssertFalse(fakePeripheralManager.isStarted);
  OCMVerifyAll(mockCBPeripheralManager);
}
*/

// Tests that the peripheral manager starts and stops correctly.
/*
- (void)testStartAndStop {
  NSDictionary *mocks = [self setupCommonMocksAndManager];
  id mockCBPeripheralManager = mocks[@"mockCBPeripheralManager"];
  GNSFakePeripheralManager *fakePeripheralManager = mocks[@"fakePeripheralManager"];
  NSMutableDictionary *mutableMocks = [mocks mutableCopy];

  GNSPeripheralServiceManager *serviceManager = nil;


  // Need to create serviceManager here as it's not part of common setup anymore.
  CBUUID *testServiceUUID = [CBUUID UUIDWithString:@"FE2C"];
  serviceManager = [[GNSPeripheralServiceManager alloc]
      initWithBleServiceUUID:testServiceUUID
    addPairingCharacteristic:NO
   shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
     return YES;
   }];

  [fakePeripheralManager start];
  XCTAssertTrue(fakePeripheralManager.isStarted);

  // When addPeripheralServiceManager is called, it will eventually call addService:.
  OCMExpect([mockCBPeripheralManager addService:[OCMArg any]])
      .andDo(^(id localSelf, CBService *service) {
        [fakePeripheralManager peripheralManager:localSelf didAddService:service error:nil];
      });
  // When updateAdvertisedServices is called (from peripheralManagerDidUpdateState),
startAdvertising: will be called. OCMExpect([mockCBPeripheralManager startAdvertising:[OCMArg any]])
      .andDo(^(id localSelf, id advertisementData) {
        [mutableMocks setObject:@(YES) forKey:@"simulatedIsAdvertising"]; // Set after
startAdvertising is called [fakePeripheralManager peripheralManagerDidStartAdvertising:localSelf
error:nil];
      });
  [fakePeripheralManager addPeripheralServiceManager:serviceManager
                            bleServiceAddedCompletion:^(NSError *error){ }];
  OCMVerifyAll(mockCBPeripheralManager); // Verify addService was called.

  // Now, simulate the peripheral manager being powered on, which should ensure advertising.
  [mutableMocks setObject:@(CBManagerStatePoweredOn) forKey:@"simulatedPeripheralManagerState"];
  [fakePeripheralManager peripheralManagerDidUpdateState:mockCBPeripheralManager];
  OCMVerifyAll(mockCBPeripheralManager); // Verify startAdvertising was called.
  XCTAssertTrue([mutableMocks[@"simulatedIsAdvertising"] boolValue]);

  OCMExpect([mockCBPeripheralManager stopAdvertising])
      .andDo(^(NSInvocation *invocation) {
        [mutableMocks setObject:@(NO) forKey:@"simulatedIsAdvertising"]; // Set after
stopAdvertising is called
      });
  OCMExpect([mockCBPeripheralManager removeAllServices]);
  OCMExpect([mockCBPeripheralManager setDelegate:nil]); // Expect setDelegate:nil
  [fakePeripheralManager stop];
  XCTAssertFalse(fakePeripheralManager.isStarted);
  OCMVerifyAll(mockCBPeripheralManager);

  // Test starting again after stopping.
  // This requires a new service manager because GNSPeripheralServiceManager cannot be reused.
  testServiceUUID = [CBUUID UUIDWithString:@"FE2C"];
  serviceManager = [[GNSPeripheralServiceManager alloc]
      initWithBleServiceUUID:testServiceUUID
    addPairingCharacteristic:NO
   shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
     return YES;
   }];

  // Add the service manager to the peripheral manager again (it was removed on stop).
  // This will call addService:.
  OCMExpect([mockCBPeripheralManager addService:[OCMArg any]])
      .andDo(^(id localSelf, CBService *service) {
        [fakePeripheralManager peripheralManager:localSelf didAddService:service error:nil];
      });
  // When updateAdvertisedServices is called (from peripheralManagerDidUpdateState),
startAdvertising: will be called. OCMExpect([mockCBPeripheralManager startAdvertising:[OCMArg any]])
      .andDo(^(id localSelf, id advertisementData) {
        [mutableMocks setObject:@(YES) forKey:@"simulatedIsAdvertising"]; // Set after
startAdvertising is called [fakePeripheralManager peripheralManagerDidStartAdvertising:localSelf
error:nil];
      });
  [fakePeripheralManager addPeripheralServiceManager:serviceManager
                            bleServiceAddedCompletion:^(NSError *error){ }];

  [fakePeripheralManager start]; // Re-start the manager
  XCTAssertTrue(fakePeripheralManager.isStarted);

  // Now, simulate the peripheral manager being powered on again.
  [mutableMocks setObject:@(CBManagerStatePoweredOn) forKey:@"simulatedPeripheralManagerState"];
  [fakePeripheralManager peripheralManagerDidUpdateState:mockCBPeripheralManager];
  XCTAssertTrue([mutableMocks[@"simulatedIsAdvertising"] boolValue]);
  OCMVerifyAll(mockCBPeripheralManager);
}
*/

// Tests that the peripheral manager handles state updates correctly.
/*
- (void)testPeripheralManagerDidUpdateState {
  NSDictionary *mocks = [self setupCommonMocksAndManager];
  id mockCBPeripheralManager = mocks[@"mockCBPeripheralManager"];
  GNSFakePeripheralManager *fakePeripheralManager = mocks[@"fakePeripheralManager"];
  NSMutableDictionary *mutableMocks = [mocks mutableCopy];

  GNSPeripheralServiceManager *serviceManager = nil;

  // Need to create serviceManager here as it's not part of common setup anymore.
  CBUUID *testServiceUUID = [CBUUID UUIDWithString:@"FE2C"];
  serviceManager = [[GNSPeripheralServiceManager alloc]
      initWithBleServiceUUID:testServiceUUID
    addPairingCharacteristic:NO
   shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
     return YES;
   }];

  [fakePeripheralManager start]; // Start the manager before adding services.
  XCTAssertTrue(fakePeripheralManager.isStarted);

  // Expect addService during initial service addition.
  OCMExpect([mockCBPeripheralManager addService:[OCMArg any]])
      .andDo(^(id localSelf, CBService *service) {
        [fakePeripheralManager peripheralManager:localSelf didAddService:service error:nil];
      });
  [fakePeripheralManager addPeripheralServiceManager:serviceManager
                            bleServiceAddedCompletion:^(NSError *error){ }];

  // Now, simulate the peripheral manager being powered on.
  // This will trigger addAllBleServicesAndStartAdvertising which calls addService: again.
  OCMExpect([mockCBPeripheralManager addService:[OCMArg any]])
      .andDo(^(id localSelf, CBService *service) {
        [fakePeripheralManager peripheralManager:localSelf didAddService:service error:nil];
      });

  // Expect startAdvertising when peripheralManagerDidUpdateState triggers advertising.
  OCMExpect([mockCBPeripheralManager startAdvertising:[OCMArg any]])
      .andDo(^(id localSelf, id advertisementData) {
        [mutableMocks setObject:@(YES) forKey:@"simulatedIsAdvertising"]; // Set after
startAdvertising is called [fakePeripheralManager peripheralManagerDidStartAdvertising:localSelf
error:nil];
      });
  [mutableMocks setObject:@(CBManagerStatePoweredOn) forKey:@"simulatedPeripheralManagerState"];
  [fakePeripheralManager peripheralManagerDidUpdateState:mockCBPeripheralManager];
  XCTAssertTrue([mutableMocks[@"simulatedIsAdvertising"] boolValue]);

  // Simulate the Bluetooth state changing to powered off.
  OCMExpect([mockCBPeripheralManager stopAdvertising]); // Expect stopAdvertising to be called.
  OCMExpect([mockCBPeripheralManager removeAllServices]);
  [mutableMocks setObject:@(CBManagerStatePoweredOff) forKey:@"simulatedPeripheralManagerState"];
  [fakePeripheralManager peripheralManagerDidUpdateState:mockCBPeripheralManager];
  XCTAssertFalse([mutableMocks[@"simulatedIsAdvertising"] boolValue]);
  OCMVerifyAll(mockCBPeripheralManager);

  // Simulate the Bluetooth state changing to powered on again.
  // This will trigger another startAdvertising call.
  [mutableMocks setObject:@(NO) forKey:@"simulatedIsAdvertising"]; // Before
updateAdvertisedServices is called
  // This also involves addService: again if the service was removed in the PoweredOff state.
  OCMExpect([mockCBPeripheralManager addService:[OCMArg any]])
      .andDo(^(id localSelf, CBService *service) {
        [fakePeripheralManager peripheralManager:localSelf didAddService:service error:nil];
      });
  OCMExpect([mockCBPeripheralManager startAdvertising:[OCMArg any]])
      .andDo(^(id localSelf, id advertisementData) {
        [mutableMocks setObject:@(YES) forKey:@"simulatedIsAdvertising"]; // Set after
startAdvertising is called [fakePeripheralManager peripheralManagerDidStartAdvertising:localSelf
error:nil];
      });
  [mutableMocks setObject:@(CBManagerStatePoweredOn) forKey:@"simulatedPeripheralManagerState"];
  [fakePeripheralManager peripheralManagerDidUpdateState:mockCBPeripheralManager];
  [fakePeripheralManager updateAdvertisedServices]; // Manually trigger update
  XCTAssertTrue([mutableMocks[@"simulatedIsAdvertising"] boolValue]);
  OCMVerifyAll(mockCBPeripheralManager);
}
*/

@end
