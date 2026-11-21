// Copyright 2023 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEMedium.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTClient.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTServer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPClient+Testing.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPFakeInputOutputStream.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEMedium+Testing.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakeCBL2CAPChannel.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakeCentralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheralManager.h"

#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "internal/flags/nearby_flags.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

static NSString *const kServiceUUID = @"0000FEF3-0000-1000-8000-00805F9B34FB";

@interface GNCMediumPendingL2CAPFakePeripheral : GNCFakePeripheral
@property(nonatomic) BOOL shouldPendOpenL2CAPChannel;
@property(nonatomic, copy, nullable) dispatch_block_t onOpenL2CAPChannel;
@end

@implementation GNCMediumPendingL2CAPFakePeripheral

- (void)openL2CAPChannel:(CBL2CAPPSM)PSM {
  if (self.onOpenL2CAPChannel) {
    self.onOpenL2CAPChannel();
  }
  if (self.shouldPendOpenL2CAPChannel) {
    return;
  }
  GNCFakeCBL2CAPChannel *channel = [[GNCFakeCBL2CAPChannel alloc] init];
  channel.inputStream = self.channelInputStream;
  channel.outputStream = self.channelOutputStream;
  channel.PSM = PSM;
  [self.peripheralDelegate peripheral:(CBPeripheral *)self
                  didOpenL2CAPChannel:(CBL2CAPChannel *)channel
                                error:self.openL2CAPChannelError];
}

@end

@interface GNCMediumAsyncCancelFakeCentralManager : GNCFakeCentralManager
@property(nonatomic) BOOL shouldPendCancelPeripheralConnection;
@property(nonatomic, readonly) NSMutableArray<id<GNCPeripheral>> *connectRequests;
@property(nonatomic, readonly) NSMutableArray<id<GNCPeripheral>> *asyncCanceledPeripherals;
@end

@implementation GNCMediumAsyncCancelFakeCentralManager {
  NSMutableArray<id<GNCPeripheral>> *_connectRequests;
  NSMutableArray<id<GNCPeripheral>> *_asyncCanceledPeripherals;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _connectRequests = [NSMutableArray array];
    _asyncCanceledPeripherals = [NSMutableArray array];
  }
  return self;
}

- (NSMutableArray<id<GNCPeripheral>> *)connectRequests {
  return _connectRequests;
}

- (NSMutableArray<id<GNCPeripheral>> *)asyncCanceledPeripherals {
  return _asyncCanceledPeripherals;
}

- (void)connectPeripheral:(id<GNCPeripheral>)peripheral
                  options:(nullable NSDictionary<NSString *, id> *)options {
  [_connectRequests addObject:peripheral];
  [super connectPeripheral:peripheral options:options];
}

- (void)cancelPeripheralConnection:(id<GNCPeripheral>)peripheral {
  [_asyncCanceledPeripherals addObject:peripheral];
  if (_shouldPendCancelPeripheralConnection) {
    return;
  }
  [super cancelPeripheralConnection:peripheral];
}

@end

@interface GNCBLEMediumTest : XCTestCase
@end

@implementation GNCBLEMediumTest

- (void)tearDown {
  nearby::NearbyFlags::GetInstance().ResetOverridedValues();
  [super tearDown];
}

- (void)testInit_allocatesMultiplexerWhenFlagEnabled {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];

    id multiplexer = [medium valueForKey:@"_multiplexer"];
    if (enabled.boolValue) {
      XCTAssertNotNil(multiplexer);
      XCTAssertEqual(fakePeripheralManager.peripheralDelegate, multiplexer);
    } else {
      XCTAssertNil(multiplexer);
      XCTAssertNil(fakePeripheralManager.peripheralDelegate);
    }
  }
}

#pragma mark - Supports Extended Advertisements

- (void)testSupportsExtendedAdvertisements {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];

    XCTAssertFalse([medium supportsExtendedAdvertisements]);
  }
}

#pragma mark - Start Scanning

- (void)testStartScanning {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    XCTestExpectation *startScanningExpectation =
        [[XCTestExpectation alloc] initWithDescription:@"Start scanning."];
    XCTestExpectation *advertisementFoundExpectation =
        [[XCTestExpectation alloc] initWithDescription:@"Advertisement found."];
    CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID];

    [fakeCentralManager simulateCentralManagerDidUpdateState:CBManagerStatePoweredOn];

    [medium startScanningForService:serviceUUID
        advertisementFoundHandler:^(id<GNCPeripheral> peripheral,
                                    NSDictionary<CBUUID *, NSData *> *data) {
          NSDictionary<CBUUID *, NSData *> *expected = @{
            serviceUUID : [@"test" dataUsingEncoding:NSUTF8StringEncoding],
          };
          XCTAssertEqualObjects(expected, data);
          [advertisementFoundExpectation fulfill];
        }
        completionHandler:^(NSError *error) {
          XCTAssertNil(error);
          [startScanningExpectation fulfill];
        }];

    [self waitForExpectations:@[ startScanningExpectation ] timeout:3];

    XCTAssertEqualObjects(@[ serviceUUID ], fakeCentralManager.serviceUUIDs);

    [fakeCentralManager
        simulateCentralManagerDidDiscoverPeripheral:[[GNCFakePeripheral alloc] init]
                                  advertisementData:@{
                                    CBAdvertisementDataLocalNameKey : @"dGVzdA",
                                    CBAdvertisementDataServiceUUIDsKey : @[ serviceUUID ],
                                  }];

    [self waitForExpectations:@[ advertisementFoundExpectation ] timeout:3];
  }
}

- (void)testAlreadyScanning {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    XCTestExpectation *expectation =
        [[XCTestExpectation alloc] initWithDescription:@"Start scanning."];

    [medium startScanningForService:[CBUUID UUIDWithString:kServiceUUID]
          advertisementFoundHandler:^(id<GNCPeripheral> peripheral,
                                      NSDictionary<CBUUID *, NSData *> *data) {
          }
                  completionHandler:nil];

    [medium startScanningForService:[CBUUID UUIDWithString:kServiceUUID]
        advertisementFoundHandler:^(id<GNCPeripheral> peripheral,
                                    NSDictionary<CBUUID *, NSData *> *data) {
        }
        completionHandler:^(NSError *error) {
          XCTAssertNotNil(error);
          [expectation fulfill];
        }];

    [self waitForExpectations:@[ expectation ] timeout:3];
  }
}

- (void)testStartStopStartScanning {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    XCTestExpectation *expectation =
        [[XCTestExpectation alloc] initWithDescription:@"Start scanning."];

    CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID];

    [medium startScanningForService:serviceUUID
        advertisementFoundHandler:^(id<GNCPeripheral> peripheral,
                                    NSDictionary<CBUUID *, NSData *> *data) {
        }
        completionHandler:^(NSError *error) {
          XCTAssertNil(error);
          [medium stopScanningWithCompletionHandler:^(NSError *error) {
            XCTAssertNil(error);
            [medium startScanningForService:serviceUUID
                advertisementFoundHandler:^(id<GNCPeripheral> peripheral,
                                            NSDictionary<CBUUID *, NSData *> *data) {
                }
                completionHandler:^(NSError *error) {
                  XCTAssertNil(error);
                  [expectation fulfill];
                }];
          }];
        }];

    [self waitForExpectations:@[ expectation ] timeout:3];
  }
}

#pragma mark - Decode Advertisement Data

- (void)testDecodeAndroidStyleAdvertisementData {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID];

    NSDictionary<CBUUID *, NSData *> *expected = @{
      serviceUUID : [@"test" dataUsingEncoding:NSUTF8StringEncoding],
    };

    NSDictionary<NSString *, id> *data = @{
      CBAdvertisementDataServiceDataKey : @{
        serviceUUID : [@"test" dataUsingEncoding:NSUTF8StringEncoding],
      },
    };
    NSDictionary<CBUUID *, NSData *> *actual = [medium decodeAdvertisementData:data];

    XCTAssertEqualObjects(expected, actual);
  }
}

- (void)testDecodeAndroidStyleAdvertisementDataWithLocalName {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID];

    NSDictionary<CBUUID *, NSData *> *expected = @{
      serviceUUID : [@"test" dataUsingEncoding:NSUTF8StringEncoding],
    };

    NSDictionary<NSString *, id> *data = @{
      CBAdvertisementDataLocalNameKey : @"Nearby",  // Just happens to be base64 decodable.
      CBAdvertisementDataServiceUUIDsKey : @[ serviceUUID ],
      CBAdvertisementDataServiceDataKey : @{
        serviceUUID : [@"test" dataUsingEncoding:NSUTF8StringEncoding],
      },
    };
    NSDictionary<CBUUID *, NSData *> *actual = [medium decodeAdvertisementData:data];

    XCTAssertEqualObjects(expected, actual);
  }
}

- (void)testDecodeAppleStyleAdvertisementData {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID];

    NSDictionary<CBUUID *, NSData *> *expected = @{
      serviceUUID : [@"test" dataUsingEncoding:NSUTF8StringEncoding],
    };

    NSDictionary<NSString *, id> *data = @{
      CBAdvertisementDataLocalNameKey : @"dGVzdA",
      CBAdvertisementDataServiceUUIDsKey : @[ serviceUUID ],
    };
    NSDictionary<CBUUID *, NSData *> *actual = [medium decodeAdvertisementData:data];

    XCTAssertEqualObjects(expected, actual);
  }
}

- (void)testDecodeInvalidAdvertisementData {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID];

    NSDictionary<NSString *, id> *data = @{
      CBAdvertisementDataLocalNameKey : @"!@#$",
      CBAdvertisementDataServiceUUIDsKey : @[ serviceUUID ],
    };
    NSDictionary<CBUUID *, NSData *> *actual = [medium decodeAdvertisementData:data];

    XCTAssertEqualObjects(@{}, actual);
  }
}

- (void)testDecodeEmptyAdvertisementData {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];

    NSDictionary<CBUUID *, NSData *> *actual = [medium decodeAdvertisementData:@{}];

    XCTAssertEqualObjects(@{}, actual);
  }
}

#pragma mark - Start GATT Server

- (void)testStartGATTServer {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    XCTestExpectation *expectation =
        [[XCTestExpectation alloc] initWithDescription:@"Start GATT server."];

    [medium startGATTServerWithCompletionHandler:^(GNCBLEGATTServer *server, NSError *error) {
      XCTAssertNotNil(server);
      XCTAssertNil(error);

      // Verify internal structure based on flag
      id mediumManager = [medium valueForKey:@"_peripheralManager"];
      id serverManager = [server valueForKey:@"_peripheralManager"];

      if (enabled.boolValue) {
        XCTAssertEqual(mediumManager, serverManager);
        id multiplexer = [medium valueForKey:@"_multiplexer"];
        XCTAssertEqual([mediumManager peripheralDelegate], multiplexer);
      } else {
        XCTAssertNotEqual(mediumManager, serverManager);
        id manager = [server valueForKey:@"_peripheralManager"];
        XCTAssertEqual([manager peripheralDelegate], server);
      }

      [expectation fulfill];
    }];

    [self waitForExpectations:@[ expectation ] timeout:3];
  }
}

#pragma mark - Start Advertising

- (void)testStartAdvertising {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    XCTestExpectation *expectation =
        [[XCTestExpectation alloc] initWithDescription:@"Start advertising."];

    // Start advertising is fully covered with @c GNCBLEGATTServer tests. We are passing invalid
    // advertising data here so we can test code paths relevant to @c GNCBLEMedium, but bail early
    // enough to avoid making actual CoreBluetooth calls.
    [medium startAdvertisingData:@{}
               completionHandler:^(NSError *error) {
                 XCTAssertNotNil(error);
                 [expectation fulfill];
               }];

    [self waitForExpectations:@[ expectation ] timeout:3];
  }
}

- (void)testStopAdvertising {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    XCTestExpectation *expectation =
        [[XCTestExpectation alloc] initWithDescription:@"Stop advertising."];

    // Stop advertising is fully covered with @c GNCBLEGATTServer tests. We are only testing
    // stopping without having started which tests the code paths relevant to @c GNCBLEMedium.
    [medium stopAdvertisingWithCompletionHandler:^(NSError *error) {
      XCTAssertNil(error);
      [expectation fulfill];
    }];

    [self waitForExpectations:@[ expectation ] timeout:3];
  }
}

#pragma mark - Open L2CAP Channel

- (void)testOpenL2CAPServerSocket {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    XCTestExpectation *psmPublishedexpectation =
        [[XCTestExpectation alloc] initWithDescription:@"PSM published."];
    XCTestExpectation *channelOpenedexpectation =
        [[XCTestExpectation alloc] initWithDescription:@"Channel opened."];

    [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

    // Open L2CAP server is fully covered with @c GNCBLEL2CAPServer tests.
    [medium
        openL2CAPServerWithPSMPublishedCompletionHandler:^(uint16_t PSM, NSError *error) {
          XCTAssertEqual(PSM, fakePeripheralManager.PSM);
          XCTAssertNil(error);
          [psmPublishedexpectation fulfill];
        }
        channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
          XCTAssertNil(error);
          [channelOpenedexpectation fulfill];
        }
        peripheralManager:fakePeripheralManager];

    [self waitForExpectations:@[ psmPublishedexpectation ] timeout:0.1];
    [self waitForExpectations:@[ channelOpenedexpectation ] timeout:0.5];
  }
}

- (void)testSuccessfulOpenL2CAPChannel {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    XCTestExpectation *expectation =
        [[XCTestExpectation alloc] initWithDescription:@"Open L2CAP channel."];

    GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];
    GNCBLEL2CAPClient *l2capClient =
        [[GNCBLEL2CAPClient alloc] initWithQueue:nil
                     requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull peripheral){
                     }];
    [medium setL2CAPClient:l2capClient];
    [medium
        openL2CAPChannelWithPSM:123
                     peripheral:fakePeripheral
              completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                [expectation fulfill];
              }];

    [self waitForExpectations:@[ expectation ] timeout:3];
  }
}

#pragma mark - Connect

- (void)testSuccessfulConnect {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    XCTestExpectation *expectation = [[XCTestExpectation alloc] initWithDescription:@"Connect."];

    [medium connectToGATTServerForPeripheral:[[GNCFakePeripheral alloc] init]
                        disconnectionHandler:nil
                           completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
                             XCTAssertNotNil(client);
                             XCTAssertNil(error);
                             [expectation fulfill];
                           }];

    [self waitForExpectations:@[ expectation ] timeout:3];
  }
}

- (void)testFailedConnect {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    XCTestExpectation *expectation = [[XCTestExpectation alloc] initWithDescription:@"Connect."];

    fakeCentralManager.didFailToConnectPeripheralError = [NSError errorWithDomain:@"fake"
                                                                             code:0
                                                                         userInfo:nil];

    [medium connectToGATTServerForPeripheral:[[GNCFakePeripheral alloc] init]
                        disconnectionHandler:nil
                           completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
                             XCTAssertNil(client);
                             XCTAssertNotNil(error);
                             [expectation fulfill];
                           }];

    [self waitForExpectations:@[ expectation ] timeout:3];
  }
}

- (void)testConnectToGATTServerTimeoutCancelsConnectionAndClearsHandler {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableBleL2cap,
      true);
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  fakeCentralManager.shouldPendConnectPeripheral = YES;
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  [medium setPeripheralConnectTimeout:0.05];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];
  XCTestExpectation *timeoutExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"GATT connect timeout."];
  __block NSUInteger completionCallCount = 0;

  [medium connectToGATTServerForPeripheral:peripheral
                      disconnectionHandler:nil
                         completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
                           completionCallCount++;
                           XCTAssertNil(client);
                           XCTAssertNotNil(error);
                           XCTAssertEqualObjects(error.domain, GNCBLEErrorDomain);
                           XCTAssertEqual(error.code, GNCBLEErrorBlePerperipheralConnectionTimeout);
                           [timeoutExpectation fulfill];
                         }];

  [self waitForExpectations:@[ timeoutExpectation ] timeout:3];
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.count, 1u);
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.firstObject, peripheral);

  // Simulate a late connection callback and verify the removed handler is not invoked again.
  [fakeCentralManager simulateCentralManagerDidConnectPeripheral:peripheral];
  XCTAssertEqual(completionCallCount, 1u);
}

- (void)testOpenL2CAPChannelTimeoutCancelsConnectionAndClearsHandler {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  fakeCentralManager.shouldPendConnectPeripheral = YES;
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  [medium setPeripheralConnectTimeout:0.05];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];
  GNCBLEL2CAPClient *l2capClient =
      [[GNCBLEL2CAPClient alloc] initWithQueue:nil
                   requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull p){
                   }];
  [medium setL2CAPClient:l2capClient];
  XCTestExpectation *timeoutExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"L2CAP connect timeout."];
  __block NSUInteger completionCallCount = 0;

  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  completionCallCount++;
                  XCTAssertNil(stream);
                  XCTAssertNotNil(error);
                  XCTAssertEqualObjects(error.domain, GNCBLEErrorDomain);
                  XCTAssertEqual(error.code, GNCBLEErrorBlePerperipheralConnectionTimeout);
                  [timeoutExpectation fulfill];
                }];

  [self waitForExpectations:@[ timeoutExpectation ] timeout:3];
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.count, 1u);
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.firstObject, peripheral);

  // Simulate a late connection callback and verify the removed L2CAP handler is not invoked again.
  [fakeCentralManager simulateCentralManagerDidConnectPeripheral:peripheral];
  XCTAssertEqual(completionCallCount, 1u);
}

- (void)testConnectToGATTServerWithNilCompletionHandlerCancelsTimeoutOnConnect {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableBleL2cap,
      true);
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  [medium setPeripheralConnectTimeout:0.05];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];

  [medium connectToGATTServerForPeripheral:peripheral
                      disconnectionHandler:nil
                         completionHandler:nil];

  XCTestExpectation *postTimeoutExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Post-timeout check."];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [postTimeoutExpectation fulfill];
                 });
  [self waitForExpectations:@[ postTimeoutExpectation ] timeout:3];

  NSDictionary *gattHandlers = [medium valueForKey:@"_gattConnectionCompletionHandlers"];
  NSDictionary *gattTimeouts = [medium valueForKey:@"_gattConnectionTimeoutBlocks"];
  XCTAssertNil(gattHandlers[peripheral.identifier]);
  XCTAssertNil(gattTimeouts[peripheral.identifier]);
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.count, 0u);
}

- (void)testOpenL2CAPChannelWithNilCompletionHandlerCancelsTimeoutOnOpen {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  [medium setPeripheralConnectTimeout:0.05];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  peripheral.channelInputStream = fakeStreams.inputStream;
  peripheral.channelOutputStream = fakeStreams.outputStream;

  [medium openL2CAPChannelWithPSM:123 peripheral:peripheral completionHandler:nil];

  XCTestExpectation *postTimeoutExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Post-timeout check."];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [postTimeoutExpectation fulfill];
                 });
  [self waitForExpectations:@[ postTimeoutExpectation ] timeout:3];

  NSDictionary *l2capHandlers = [medium valueForKey:@"_l2capStreamCompletionHandlers"];
  NSDictionary *l2capAttempts = [medium valueForKey:@"_l2capOpenAttempts"];
  NSDictionary *l2capTimeouts = [medium valueForKey:@"_l2capConnectionTimeoutBlocks"];
  XCTAssertNil(l2capHandlers[peripheral.identifier]);
  XCTAssertNil(l2capAttempts[peripheral.identifier]);
  XCTAssertNil(l2capTimeouts[peripheral.identifier]);
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.count, 0u);
  [fakeStreams tearDown];
}

- (void)testDisconnect {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    XCTestExpectation *disconnectExpectation =
        [[XCTestExpectation alloc] initWithDescription:@"Disconnect."];

    GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];

    [medium connectToGATTServerForPeripheral:peripheral
        disconnectionHandler:^() {
          [disconnectExpectation fulfill];
        }
        completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
          XCTAssertNotNil(client);
          XCTAssertNil(error);
          [client disconnect];
        }];

    [self waitForExpectations:@[ disconnectExpectation ] timeout:3];
  }
}

- (void)testRetrievePeripheralWithIdentifier_exists {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    XCTAssertNotNil(
        [medium retrievePeripheralWithIdentifier:
                    [[NSUUID alloc] initWithUUIDString:@"11111111-1111-1111-1111-111111111111"]]);
  }
}

- (void)testRetrievePeripheralWithIdentifier_doesNotExist {
  for (NSNumber *enabled in @[ @NO, @YES ]) {
    nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
        nearby::connections::config_package_nearby::nearby_connections_feature::
            kEnableSharedPeripheralManager,
        enabled.boolValue);
    GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    XCTAssertNil(
        [medium retrievePeripheralWithIdentifier:
                    [[NSUUID alloc] initWithUUIDString:@"11111111-1111-1111-1111-111111111112"]]);
  }
}

- (void)testOpeningL2CAPDoesNotBlockGATTCharacteristicDiscovery {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];

  __block GNCBLEGATTClient *gattClient = nil;
  XCTestExpectation *gattExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"GATT connected"];
  [medium connectToGATTServerForPeripheral:peripheral
                      disconnectionHandler:nil
                         completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
                           XCTAssertNil(error);
                           gattClient = client;
                           [gattExpectation fulfill];
                         }];
  [self waitForExpectations:@[ gattExpectation ] timeout:3];
  XCTAssertNotNil(gattClient);

  // Delay peripheral delegate callbacks so L2CAP opens while GATT discovery is in flight.
  peripheral.delegateDelay = 0.05;
  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:@"0000FEF4-0000-1000-8000-00805F9B34FB"];
  XCTestExpectation *discoverExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"GATT discovery completed"];
  [gattClient discoverCharacteristicsWithUUIDs:@[ characteristicUUID ]
                                   serviceUUID:serviceUUID
                             completionHandler:^(NSError *error) {
                               XCTAssertNil(error);
                               [discoverExpectation fulfill];
                             }];

  XCTestExpectation *l2capExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"L2CAP opened"];
  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  XCTAssertNil(error);
                  [l2capExpectation fulfill];
                }];

  [self waitForExpectations:@[ discoverExpectation, l2capExpectation ] timeout:3];
}

- (void)testL2CAPCloseCancelsReportedPeripheralNotFirstPeripheral {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];

  GNCFakePeripheral *peripheral1 = [[GNCFakePeripheral alloc] init];
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams1 =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  peripheral1.channelInputStream = fakeStreams1.inputStream;
  peripheral1.channelOutputStream = fakeStreams1.outputStream;

  GNCFakePeripheral *peripheral2 = [[GNCFakePeripheral alloc] init];
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams2 =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  peripheral2.channelInputStream = fakeStreams2.inputStream;
  peripheral2.channelOutputStream = fakeStreams2.outputStream;

  XCTestExpectation *open1 = [[XCTestExpectation alloc] initWithDescription:@"Open 1"];
  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral1
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  XCTAssertNotNil(stream);
                  [open1 fulfill];
                }];
  [self waitForExpectations:@[ open1 ] timeout:3];

  __block GNCBLEL2CAPStream *stream2 = nil;
  XCTestExpectation *open2 = [[XCTestExpectation alloc] initWithDescription:@"Open 2"];
  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral2
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  XCTAssertNotNil(stream);
                  stream2 = stream;
                  [open2 fulfill];
                }];
  [self waitForExpectations:@[ open2 ] timeout:3];

  [stream2 close];
  XCTestExpectation *drainExpectation = [[XCTestExpectation alloc] initWithDescription:@"Drain"];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.05 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [drainExpectation fulfill];
                 });
  [self waitForExpectations:@[ drainExpectation ] timeout:3];
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.count, 1u);
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.firstObject, peripheral2);
}

- (void)testL2CAPCloseThenReopenSamePeripheralDoesNotCancelNewAttempt {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];

  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams1 =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  peripheral.channelInputStream = fakeStreams1.inputStream;
  peripheral.channelOutputStream = fakeStreams1.outputStream;

  __block GNCBLEL2CAPStream *stream1 = nil;
  XCTestExpectation *open1 = [[XCTestExpectation alloc] initWithDescription:@"Open 1"];
  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  XCTAssertNotNil(stream);
                  XCTAssertNil(error);
                  stream1 = stream;
                  [open1 fulfill];
                }];
  [self waitForExpectations:@[ open1 ] timeout:3];

  NSDictionary<NSUUID *, NSSet<GNCBLEL2CAPClient *> *> *clients =
      [medium valueForKey:@"_l2capClients"];
  GNCBLEL2CAPClient *firstClient = clients[peripheral.identifier].anyObject;
  XCTAssertNotNil(firstClient);
  dispatch_queue_t firstClientQueue = [firstClient valueForKey:@"_queue"];

  // Hold firstClient's serial queue so its async disconnect hops land on medium's queue after the
  // reopen attempt for the same peripheral has already been enqueued.
  dispatch_semaphore_t clientQueueReady = dispatch_semaphore_create(0);
  dispatch_semaphore_t releaseClientQueue = dispatch_semaphore_create(0);
  dispatch_async(firstClientQueue, ^{
    dispatch_semaphore_signal(clientQueueReady);
    dispatch_semaphore_wait(releaseClientQueue, DISPATCH_TIME_FOREVER);
  });
  dispatch_semaphore_wait(clientQueueReady, DISPATCH_TIME_FOREVER);

  GNCBLEL2CAPFakeInputOutputStream *fakeStreams2 =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  peripheral.channelInputStream = fakeStreams2.inputStream;
  peripheral.channelOutputStream = fakeStreams2.outputStream;

  [stream1 close];

  __block GNCBLEL2CAPStream *stream2 = nil;
  XCTestExpectation *open2 = [[XCTestExpectation alloc] initWithDescription:@"Open 2"];
  [medium openL2CAPChannelWithPSM:456
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  XCTAssertNotNil(stream);
                  XCTAssertNil(error);
                  stream2 = stream;
                  [open2 fulfill];
                }];

  // Allow firstClient's disconnect to run and enqueue its requestDisconnectionHandler behind open2.
  dispatch_semaphore_t clientDisconnectProcessed = dispatch_semaphore_create(0);
  dispatch_async(firstClientQueue, ^{
    dispatch_semaphore_signal(clientDisconnectProcessed);
  });
  dispatch_semaphore_signal(releaseClientQueue);
  dispatch_semaphore_wait(clientDisconnectProcessed, DISPATCH_TIME_FOREVER);

  XCTestExpectation *staleDisconnectDrained =
      [[XCTestExpectation alloc] initWithDescription:@"Stale disconnect drained"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [staleDisconnectDrained fulfill];
  });

  [self waitForExpectations:@[ open2, staleDisconnectDrained ] timeout:3];

  XCTAssertEqual(clients[peripheral.identifier].count, 1u);
  GNCBLEL2CAPClient *secondClient = clients[peripheral.identifier].anyObject;
  XCTAssertNotNil(secondClient);
  XCTAssertNotEqual(firstClient, secondClient);
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.count, 0u);

  // Closing the new stream should still disconnect the peripheral and clean up the active client.
  dispatch_queue_t secondClientQueue = [secondClient valueForKey:@"_queue"];
  XCTAssertNotNil(secondClientQueue);
  if (secondClientQueue) {
    [stream2 close];
    XCTestExpectation *secondDisconnectDrained =
        [[XCTestExpectation alloc] initWithDescription:@"Second disconnect drained"];
    dispatch_async(secondClientQueue, ^{
      dispatch_async(dispatch_get_main_queue(), ^{
        [secondDisconnectDrained fulfill];
      });
    });
    [self waitForExpectations:@[ secondDisconnectDrained ] timeout:3];
  }
  XCTAssertNil(clients[peripheral.identifier]);
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.count, 1u);
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.firstObject, peripheral);

  [fakeStreams1 tearDown];
  [fakeStreams2 tearDown];
}

- (void)testDisconnectFailsPendingGATTAndL2CAPHandlersWithFallbackErrorAndClearsState {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableBleL2cap,
      true);
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  fakeCentralManager.shouldPendConnectPeripheral = YES;
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  [medium setPeripheralConnectTimeout:0.05];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];

  XCTestExpectation *gattExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Pending GATT failed on disconnect."];
  XCTestExpectation *l2capExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Pending L2CAP failed on disconnect."];
  XCTestExpectation *disconnectExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"GATT disconnection handler invoked."];
  __block NSUInteger gattCompletionCallCount = 0;
  __block NSUInteger l2capCompletionCallCount = 0;

  [medium connectToGATTServerForPeripheral:peripheral
      disconnectionHandler:^{
        [disconnectExpectation fulfill];
      }
      completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
        gattCompletionCallCount++;
        XCTAssertNil(client);
        XCTAssertNotNil(error);
        XCTAssertEqualObjects(error.domain, GNCBLEErrorDomain);
        XCTAssertEqual(error.code, GNCBLEErrorUnknown);
        [gattExpectation fulfill];
      }];

  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  l2capCompletionCallCount++;
                  XCTAssertNil(stream);
                  XCTAssertNotNil(error);
                  XCTAssertEqualObjects(error.domain, GNCBLEErrorDomain);
                  XCTAssertEqual(error.code, GNCBLEErrorUnknown);
                  [l2capExpectation fulfill];
                }];

  dispatch_async(dispatch_get_main_queue(), ^{
    NSDictionary *gattHandlers = [medium valueForKey:@"_gattConnectionCompletionHandlers"];
    NSDictionary *l2capHandlers = [medium valueForKey:@"_l2capStreamCompletionHandlers"];
    NSDictionary *l2capPSMs = [medium valueForKey:@"_l2capPSMs"];
    XCTAssertNotNil(gattHandlers[peripheral.identifier]);
    XCTAssertNotNil(l2capHandlers[peripheral.identifier]);
    XCTAssertEqualObjects(l2capPSMs[peripheral.identifier], @(123));

    [fakeCentralManager simulateCentralManagerDidDisconnectPeripheral:peripheral];
  });

  [self waitForExpectations:@[ gattExpectation, l2capExpectation, disconnectExpectation ]
                    timeout:3];

  NSDictionary *gattHandlers = [medium valueForKey:@"_gattConnectionCompletionHandlers"];
  NSDictionary *l2capHandlers = [medium valueForKey:@"_l2capStreamCompletionHandlers"];
  NSDictionary *l2capPSMs = [medium valueForKey:@"_l2capPSMs"];
  NSDictionary *l2capClients = [medium valueForKey:@"_l2capClients"];
  NSDictionary *gattTimeouts = [medium valueForKey:@"_gattConnectionTimeoutBlocks"];
  NSDictionary *l2capTimeouts = [medium valueForKey:@"_l2capConnectionTimeoutBlocks"];
  XCTAssertNil(gattHandlers[peripheral.identifier]);
  XCTAssertNil(l2capHandlers[peripheral.identifier]);
  XCTAssertNil(l2capPSMs[peripheral.identifier]);
  XCTAssertNil(l2capClients[peripheral.identifier]);
  XCTAssertNil(gattTimeouts[peripheral.identifier]);
  XCTAssertNil(l2capTimeouts[peripheral.identifier]);

  // Wait past the 0.05s connection timeout and simulate a late connect to verify neither fires
  // stale callbacks.
  XCTestExpectation *postTimeoutExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Post-timeout check"];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [fakeCentralManager simulateCentralManagerDidConnectPeripheral:peripheral];
                   [postTimeoutExpectation fulfill];
                 });
  [self waitForExpectations:@[ postTimeoutExpectation ] timeout:3];
  XCTAssertEqual(gattCompletionCallCount, 1u);
  XCTAssertEqual(l2capCompletionCallCount, 1u);
}

- (void)testDisconnectPropagatesUnderlyingErrorToPendingGATTAndL2CAPHandlers {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  fakeCentralManager.shouldPendConnectPeripheral = YES;
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];
  NSError *expectedError = [NSError errorWithDomain:CBErrorDomain
                                               code:CBErrorPeripheralDisconnected
                                           userInfo:nil];

  XCTestExpectation *gattExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Pending GATT received disconnect error."];
  XCTestExpectation *l2capExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Pending L2CAP received disconnect error."];

  [medium connectToGATTServerForPeripheral:peripheral
                      disconnectionHandler:nil
                         completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
                           XCTAssertNil(client);
                           XCTAssertEqualObjects(error, expectedError);
                           [gattExpectation fulfill];
                         }];

  [medium openL2CAPChannelWithPSM:456
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  XCTAssertNil(stream);
                  XCTAssertEqualObjects(error, expectedError);
                  [l2capExpectation fulfill];
                }];

  dispatch_async(dispatch_get_main_queue(), ^{
    [fakeCentralManager.centralDelegate gnc_centralManager:fakeCentralManager
                                   didDisconnectPeripheral:peripheral
                                                     error:expectedError];
  });

  [self waitForExpectations:@[ gattExpectation, l2capExpectation ] timeout:3];
}

- (void)testDisconnectWhileOpenL2CAPChannelInFlightInvokesHandlerOnceAndDisconnectsClient {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  GNCMediumPendingL2CAPFakePeripheral *peripheral =
      [[GNCMediumPendingL2CAPFakePeripheral alloc] init];
  peripheral.shouldPendOpenL2CAPChannel = YES;

  XCTestExpectation *channelOpenStartedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"openL2CAPChannel started on peripheral"];
  peripheral.onOpenL2CAPChannel = ^{
    [channelOpenStartedExpectation fulfill];
  };

  NSError *expectedError = [NSError errorWithDomain:CBErrorDomain
                                               code:CBErrorPeripheralDisconnected
                                           userInfo:nil];
  XCTestExpectation *l2capExpectation = [[XCTestExpectation alloc]
      initWithDescription:@"In-flight L2CAP failed with disconnect error"];
  __block NSUInteger completionCallCount = 0;

  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  completionCallCount++;
                  XCTAssertNil(stream);
                  XCTAssertEqualObjects(error, expectedError);
                  [l2capExpectation fulfill];
                }];

  [self waitForExpectations:@[ channelOpenStartedExpectation ] timeout:1];

  NSDictionary<NSUUID *, NSSet<GNCBLEL2CAPClient *> *> *clientsMap =
      [medium valueForKey:@"_l2capClients"];
  GNCBLEL2CAPClient *heldClient = clientsMap[peripheral.identifier].anyObject;
  XCTAssertNotNil(heldClient);
  id<GNCPeripheralDelegate> delegateBeforeDisconnect = peripheral.peripheralDelegate;

  [fakeCentralManager.centralDelegate gnc_centralManager:fakeCentralManager
                                 didDisconnectPeripheral:peripheral
                                                   error:expectedError];

  [self waitForExpectations:@[ l2capExpectation ] timeout:0.5];

  NSDictionary *l2capHandlers = [medium valueForKey:@"_l2capStreamCompletionHandlers"];
  NSDictionary *l2capTimeouts = [medium valueForKey:@"_l2capConnectionTimeoutBlocks"];
  XCTAssertNil(clientsMap[peripheral.identifier]);
  XCTAssertNil(l2capHandlers[peripheral.identifier]);
  XCTAssertNil(l2capTimeouts[peripheral.identifier]);

  // Simulate a late didOpenL2CAPChannel callback and drain the client queue to verify disconnect
  // cleared the client state and handler is not invoked a second time.
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  GNCFakeCBL2CAPChannel *lateChannel = [[GNCFakeCBL2CAPChannel alloc] init];
  lateChannel.inputStream = fakeStreams.inputStream;
  lateChannel.outputStream = fakeStreams.outputStream;
  lateChannel.PSM = 123;
  [delegateBeforeDisconnect peripheral:(CBPeripheral *)peripheral
                   didOpenL2CAPChannel:(CBL2CAPChannel *)lateChannel
                                 error:nil];

  dispatch_queue_t clientQueue = [heldClient valueForKey:@"_queue"];
  if (clientQueue) {
    dispatch_sync(clientQueue, ^{
                  });
  }
  XCTestExpectation *drainExpectation = [[XCTestExpectation alloc] initWithDescription:@"Drain"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [drainExpectation fulfill];
  });
  [self waitForExpectations:@[ drainExpectation ] timeout:1];

  XCTAssertNil([heldClient valueForKey:@"_peripheral"]);
  XCTAssertNil([heldClient valueForKey:@"_completionHandler"]);
  XCTAssertEqual(completionCallCount, 1u);
  [fakeStreams tearDown];
}

- (void)testOpenL2CAPChannelStallAfterConnectTimesOutInvokesHandlerOnceAndDisconnectsClient {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  [medium setPeripheralConnectTimeout:0.05];

  GNCMediumPendingL2CAPFakePeripheral *peripheral =
      [[GNCMediumPendingL2CAPFakePeripheral alloc] init];
  peripheral.shouldPendOpenL2CAPChannel = YES;

  XCTestExpectation *channelOpenStartedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"openL2CAPChannel started on peripheral"];
  peripheral.onOpenL2CAPChannel = ^{
    [channelOpenStartedExpectation fulfill];
  };

  XCTestExpectation *timeoutExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Stalled L2CAP open timed out"];
  __block NSUInteger completionCallCount = 0;

  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  completionCallCount++;
                  XCTAssertNil(stream);
                  XCTAssertNotNil(error);
                  XCTAssertEqualObjects(error.domain, GNCBLEErrorDomain);
                  XCTAssertEqual(error.code, GNCBLEErrorBlePerperipheralConnectionTimeout);
                  [timeoutExpectation fulfill];
                }];

  [self waitForExpectations:@[ channelOpenStartedExpectation ] timeout:1];

  NSDictionary<NSUUID *, NSSet<GNCBLEL2CAPClient *> *> *clientsMap =
      [medium valueForKey:@"_l2capClients"];
  GNCBLEL2CAPClient *heldClient = clientsMap[peripheral.identifier].anyObject;
  XCTAssertNotNil(heldClient);

  [self waitForExpectations:@[ timeoutExpectation ] timeout:0.5];

  XCTAssertEqual(fakeCentralManager.canceledPeripherals.count, 1u);
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.firstObject, peripheral);
  XCTAssertNil(clientsMap[peripheral.identifier]);

  dispatch_queue_t clientQueue = [heldClient valueForKey:@"_queue"];
  if (clientQueue) {
    dispatch_sync(clientQueue, ^{
                  });
  }
  XCTAssertNil([heldClient valueForKey:@"_peripheral"]);
  XCTAssertNil([heldClient valueForKey:@"_completionHandler"]);
  XCTAssertEqual(completionCallCount, 1u);
}

- (void)testDisconnectRacingWithQueuedOpenL2CAPCompletionInvokesHandlerOnce {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  GNCMediumPendingL2CAPFakePeripheral *peripheral =
      [[GNCMediumPendingL2CAPFakePeripheral alloc] init];
  peripheral.shouldPendOpenL2CAPChannel = YES;

  XCTestExpectation *channelOpenStartedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"openL2CAPChannel started"];
  peripheral.onOpenL2CAPChannel = ^{
    [channelOpenStartedExpectation fulfill];
  };

  NSError *expectedError = [NSError errorWithDomain:CBErrorDomain
                                               code:CBErrorPeripheralDisconnected
                                           userInfo:nil];
  XCTestExpectation *completionExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Completion invoked once"];
  __block NSUInteger completionCallCount = 0;

  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  completionCallCount++;
                  XCTAssertNil(stream);
                  XCTAssertEqualObjects(error, expectedError);
                  [completionExpectation fulfill];
                }];

  [self waitForExpectations:@[ channelOpenStartedExpectation ] timeout:1];

  NSDictionary<NSUUID *, NSSet<GNCBLEL2CAPClient *> *> *clientsMap =
      [medium valueForKey:@"_l2capClients"];
  GNCBLEL2CAPClient *client = clientsMap[peripheral.identifier].anyObject;
  XCTAssertNotNil(client);
  dispatch_queue_t clientQueue = [client valueForKey:@"_queue"];
  XCTAssertNotNil(clientQueue);

  // Deliver didOpenL2CAPChannel on clientQueue so client enqueues its completion hop onto main
  // queue, then process didDisconnectPeripheral on main queue before that hop executes.
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  GNCFakeCBL2CAPChannel *channel = [[GNCFakeCBL2CAPChannel alloc] init];
  channel.inputStream = fakeStreams.inputStream;
  channel.outputStream = fakeStreams.outputStream;
  channel.PSM = 123;
  [peripheral.peripheralDelegate peripheral:(CBPeripheral *)peripheral
                        didOpenL2CAPChannel:(CBL2CAPChannel *)channel
                                      error:nil];
  if (clientQueue) {
    dispatch_sync(clientQueue, ^{
                  });
  }

  [fakeCentralManager.centralDelegate gnc_centralManager:fakeCentralManager
                                 didDisconnectPeripheral:peripheral
                                                   error:expectedError];

  XCTestExpectation *drainExpectation = [[XCTestExpectation alloc] initWithDescription:@"Drain"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [drainExpectation fulfill];
  });
  [self waitForExpectations:@[ completionExpectation, drainExpectation ] timeout:1];
  XCTAssertEqual(completionCallCount, 1u);
  [fakeStreams tearDown];
}

- (void)testOpenL2CAPChannelWithNilStreamAndNilErrorRemovesClientFromMap {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  GNCMediumPendingL2CAPFakePeripheral *peripheral =
      [[GNCMediumPendingL2CAPFakePeripheral alloc] init];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Completed with nil stream and nil error"];
  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  XCTAssertNil(stream);
                  XCTAssertNil(error);
                  [expectation fulfill];
                }];
  [self waitForExpectations:@[ expectation ] timeout:1];

  NSDictionary<NSUUID *, NSSet<GNCBLEL2CAPClient *> *> *clientsMap =
      [medium valueForKey:@"_l2capClients"];
  XCTAssertNil(clientsMap[peripheral.identifier]);
}

- (void)testSecondOpenL2CAPChannelOnSamePeripheralPreservesEarlierOpenClientAndChannel {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  GNCMediumPendingL2CAPFakePeripheral *peripheral =
      [[GNCMediumPendingL2CAPFakePeripheral alloc] init];
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams1 =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  peripheral.channelInputStream = fakeStreams1.inputStream;
  peripheral.channelOutputStream = fakeStreams1.outputStream;

  __block GNCBLEL2CAPStream *stream1 = nil;
  XCTestExpectation *open1 = [[XCTestExpectation alloc] initWithDescription:@"Open 1"];
  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  XCTAssertNotNil(stream);
                  XCTAssertNil(error);
                  stream1 = stream;
                  [open1 fulfill];
                }];
  [self waitForExpectations:@[ open1 ] timeout:1];

  __weak GNCBLEL2CAPClient *weakFirstClient = nil;
  @autoreleasepool {
    NSDictionary<NSUUID *, NSSet<GNCBLEL2CAPClient *> *> *clientsMap =
        [medium valueForKey:@"_l2capClients"];
    GNCBLEL2CAPClient *firstClient = clientsMap[peripheral.identifier].anyObject;
    XCTAssertNotNil(firstClient);
    weakFirstClient = firstClient;
  }
  XCTAssertNotNil(weakFirstClient);

  GNCBLEL2CAPFakeInputOutputStream *fakeStreams2 =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  peripheral.channelInputStream = fakeStreams2.inputStream;
  peripheral.channelOutputStream = fakeStreams2.outputStream;

  __block GNCBLEL2CAPStream *stream2 = nil;
  XCTestExpectation *open2 = [[XCTestExpectation alloc] initWithDescription:@"Open 2"];
  @autoreleasepool {
    [medium openL2CAPChannelWithPSM:456
                         peripheral:peripheral
                  completionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                      NSError *_Nullable error) {
                    XCTAssertNotNil(stream);
                    XCTAssertNil(error);
                    stream2 = stream;
                    [open2 fulfill];
                  }];
    [self waitForExpectations:@[ open2 ] timeout:1];
  }

  // Opening a second channel on the same peripheral must not deallocate the earlier open client.
  XCTAssertNotNil(weakFirstClient);

  // Closing stream1 while stream2 remains open must not cancel the peripheral connection.
  [stream1 close];
  XCTestExpectation *firstDrain = [[XCTestExpectation alloc] initWithDescription:@"First drain"];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.05 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [firstDrain fulfill];
                 });
  [self waitForExpectations:@[ firstDrain ] timeout:1];
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.count, 0u);

  // Closing stream2 releases the remaining client and cancels the peripheral connection.
  [stream2 close];
  XCTestExpectation *secondDrain = [[XCTestExpectation alloc] initWithDescription:@"Second drain"];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.05 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [secondDrain fulfill];
                 });
  [self waitForExpectations:@[ secondDrain ] timeout:1];
  NSDictionary<NSUUID *, NSSet<GNCBLEL2CAPClient *> *> *clientsMap =
      [medium valueForKey:@"_l2capClients"];
  XCTAssertNil(clientsMap[peripheral.identifier]);
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.count, 1u);
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.firstObject, peripheral);

  [fakeStreams1 tearDown];
  [fakeStreams2 tearDown];
}

- (void)testSupersedingInFlightOpenL2CAPChannelFailsPreviousHandlerOnceAndDisconnectsClient {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  GNCMediumPendingL2CAPFakePeripheral *peripheral =
      [[GNCMediumPendingL2CAPFakePeripheral alloc] init];
  peripheral.shouldPendOpenL2CAPChannel = YES;

  XCTestExpectation *firstStartedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"First openL2CAPChannel started"];
  peripheral.onOpenL2CAPChannel = ^{
    [firstStartedExpectation fulfill];
  };

  XCTestExpectation *firstFailedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"First in-flight attempt superseded"];
  __block NSUInteger firstCallCount = 0;
  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  firstCallCount++;
                  XCTAssertNil(stream);
                  XCTAssertNotNil(error);
                  XCTAssertEqualObjects(error.domain, GNCBLEErrorDomain);
                  XCTAssertEqual(error.code, GNCBLEErrorUnknown);
                  [firstFailedExpectation fulfill];
                }];

  [self waitForExpectations:@[ firstStartedExpectation ] timeout:1];

  NSDictionary<NSUUID *, NSSet<GNCBLEL2CAPClient *> *> *clientsMap =
      [medium valueForKey:@"_l2capClients"];
  GNCBLEL2CAPClient *firstClient = clientsMap[peripheral.identifier].anyObject;
  XCTAssertNotNil(firstClient);

  peripheral.onOpenL2CAPChannel = nil;
  peripheral.shouldPendOpenL2CAPChannel = NO;
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  peripheral.channelInputStream = fakeStreams.inputStream;
  peripheral.channelOutputStream = fakeStreams.outputStream;

  __block GNCBLEL2CAPStream *secondStream = nil;
  XCTestExpectation *secondOpenedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Second attempt opened"];
  [medium openL2CAPChannelWithPSM:456
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  XCTAssertNotNil(stream);
                  XCTAssertNil(error);
                  secondStream = stream;
                  [secondOpenedExpectation fulfill];
                }];

  [self waitForExpectations:@[ firstFailedExpectation, secondOpenedExpectation ] timeout:1];

  dispatch_queue_t firstClientQueue = [firstClient valueForKey:@"_queue"];
  if (firstClientQueue) {
    dispatch_sync(firstClientQueue, ^{
                  });
  }
  XCTestExpectation *drainExpectation = [[XCTestExpectation alloc] initWithDescription:@"Drain"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [drainExpectation fulfill];
  });
  [self waitForExpectations:@[ drainExpectation ] timeout:1];

  XCTAssertEqual(firstCallCount, 1u);
  XCTAssertNil([firstClient valueForKey:@"_peripheral"]);
  XCTAssertNil([firstClient valueForKey:@"_completionHandler"]);
  XCTAssertEqual(fakeCentralManager.canceledPeripherals.count, 0u);

  [secondStream tearDown];
  [fakeStreams tearDown];
}

- (void)testAsyncCancelCompletionStartsQueuedOpenL2CAPChannelInsteadOfFailing {
  GNCMediumAsyncCancelFakeCentralManager *fakeCentralManager =
      [[GNCMediumAsyncCancelFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  peripheral.channelInputStream = fakeStreams.inputStream;
  peripheral.channelOutputStream = fakeStreams.outputStream;

  __block GNCBLEGATTClient *gattClient = nil;
  __block NSUInteger gattDisconnectCount = 0;
  XCTestExpectation *gattConnectedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"GATT connected"];
  [medium connectToGATTServerForPeripheral:peripheral
      disconnectionHandler:^{
        gattDisconnectCount++;
      }
      completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
        XCTAssertNotNil(client);
        XCTAssertNil(error);
        gattClient = client;
        [gattConnectedExpectation fulfill];
      }];
  [self waitForExpectations:@[ gattConnectedExpectation ] timeout:1];
  XCTAssertEqual(fakeCentralManager.connectRequests.count, 1u);

  // Simulate real CoreBluetooth where cancelPeripheralConnection: delivers didDisconnectPeripheral:
  // asynchronously after openL2CAPChannelWithPSM: has already been called.
  fakeCentralManager.shouldPendCancelPeripheralConnection = YES;
  [gattClient disconnect];

  dispatch_queue_t gattClientQueue = [gattClient valueForKey:@"_queue"];
  if (gattClientQueue) {
    dispatch_sync(gattClientQueue, ^{
                  });
  }
  XCTestExpectation *cancelDispatchedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Cancel dispatched"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [cancelDispatchedExpectation fulfill];
  });
  [self waitForExpectations:@[ cancelDispatchedExpectation ] timeout:1];
  XCTAssertEqual(fakeCentralManager.asyncCanceledPeripherals.count, 1u);
  XCTAssertEqual(gattDisconnectCount, 0u);

  __block GNCBLEL2CAPStream *openedStream = nil;
  XCTestExpectation *l2capOpenedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Queued L2CAP opened after disconnect"];
  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  XCTAssertNotNil(stream);
                  XCTAssertNil(error);
                  openedStream = stream;
                  [l2capOpenedExpectation fulfill];
                }];

  XCTestExpectation *l2capQueuedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"L2CAP queued behind pending cancel"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [l2capQueuedExpectation fulfill];
  });
  [self waitForExpectations:@[ l2capQueuedExpectation ] timeout:1];
  // connectPeripheral: must be deferred until the prior cancel finishes disconnecting.
  XCTAssertEqual(fakeCentralManager.connectRequests.count, 1u);

  // Deliver the delayed didDisconnectPeripheral:error:nil from the earlier GATT disconnect.
  [fakeCentralManager simulateCentralManagerDidDisconnectPeripheral:peripheral];
  [self waitForExpectations:@[ l2capOpenedExpectation ] timeout:1];

  XCTAssertEqual(gattDisconnectCount, 1u);
  XCTAssertEqual(fakeCentralManager.connectRequests.count, 2u);
  XCTAssertNotNil(openedStream);

  [openedStream tearDown];
  [fakeStreams tearDown];
}

- (void)testAsyncCancelCompletionStartsQueuedGATTConnectAndPreservesDisconnectionHandlers {
  GNCMediumAsyncCancelFakeCentralManager *fakeCentralManager =
      [[GNCMediumAsyncCancelFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];

  __block GNCBLEGATTClient *firstClient = nil;
  __block NSUInteger firstDisconnectCount = 0;
  XCTestExpectation *firstConnected =
      [[XCTestExpectation alloc] initWithDescription:@"First GATT connected"];
  [medium connectToGATTServerForPeripheral:peripheral
      disconnectionHandler:^{
        firstDisconnectCount++;
      }
      completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
        XCTAssertNotNil(client);
        XCTAssertNil(error);
        firstClient = client;
        [firstConnected fulfill];
      }];
  [self waitForExpectations:@[ firstConnected ] timeout:1];

  fakeCentralManager.shouldPendCancelPeripheralConnection = YES;
  [firstClient disconnect];

  dispatch_queue_t firstClientQueue = [firstClient valueForKey:@"_queue"];
  if (firstClientQueue) {
    dispatch_sync(firstClientQueue, ^{
                  });
  }
  XCTestExpectation *cancelDispatched =
      [[XCTestExpectation alloc] initWithDescription:@"Cancel dispatched"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [cancelDispatched fulfill];
  });
  [self waitForExpectations:@[ cancelDispatched ] timeout:1];
  XCTAssertEqual(fakeCentralManager.asyncCanceledPeripherals.count, 1u);

  __block GNCBLEGATTClient *secondClient = nil;
  __block NSUInteger secondDisconnectCount = 0;
  XCTestExpectation *secondConnected =
      [[XCTestExpectation alloc] initWithDescription:@"Second GATT connected"];
  [medium connectToGATTServerForPeripheral:peripheral
      disconnectionHandler:^{
        secondDisconnectCount++;
      }
      completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
        XCTAssertNotNil(client);
        XCTAssertNil(error);
        secondClient = client;
        [secondConnected fulfill];
      }];

  XCTestExpectation *queuedDrain = [[XCTestExpectation alloc] initWithDescription:@"Queued drain"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [queuedDrain fulfill];
  });
  [self waitForExpectations:@[ queuedDrain ] timeout:1];
  XCTAssertEqual(fakeCentralManager.connectRequests.count, 1u);
  XCTAssertEqual(firstDisconnectCount, 0u);
  XCTAssertEqual(secondDisconnectCount, 0u);

  // Delayed didDisconnectPeripheral: completes the first session and connects the second.
  [fakeCentralManager simulateCentralManagerDidDisconnectPeripheral:peripheral];
  [self waitForExpectations:@[ secondConnected ] timeout:1];

  XCTAssertEqual(firstDisconnectCount, 1u);
  XCTAssertEqual(secondDisconnectCount, 0u);
  XCTAssertEqual(fakeCentralManager.connectRequests.count, 2u);
  XCTAssertNotNil(secondClient);

  // Subsequent disconnect invokes only the second session's disconnection handler.
  [fakeCentralManager simulateCentralManagerDidDisconnectPeripheral:peripheral];
  XCTestExpectation *secondDisconnectDrain =
      [[XCTestExpectation alloc] initWithDescription:@"Second disconnect drain"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [secondDisconnectDrain fulfill];
  });
  [self waitForExpectations:@[ secondDisconnectDrain ] timeout:1];
  XCTAssertEqual(firstDisconnectCount, 1u);
  XCTAssertEqual(secondDisconnectCount, 1u);
}

- (void)testQueuedL2CAPAttemptTimeoutDuringPendingCancelDoesNotReconnectOnDisconnect {
  GNCMediumAsyncCancelFakeCentralManager *fakeCentralManager =
      [[GNCMediumAsyncCancelFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  [medium setPeripheralConnectTimeout:0.05];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];

  __block GNCBLEGATTClient *gattClient = nil;
  XCTestExpectation *gattConnected =
      [[XCTestExpectation alloc] initWithDescription:@"GATT connected"];
  [medium connectToGATTServerForPeripheral:peripheral
                      disconnectionHandler:nil
                         completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
                           XCTAssertNotNil(client);
                           gattClient = client;
                           [gattConnected fulfill];
                         }];
  [self waitForExpectations:@[ gattConnected ] timeout:1];

  fakeCentralManager.shouldPendCancelPeripheralConnection = YES;
  [gattClient disconnect];
  dispatch_queue_t gattClientQueue = [gattClient valueForKey:@"_queue"];
  if (gattClientQueue) {
    dispatch_sync(gattClientQueue, ^{
                  });
  }
  XCTestExpectation *cancelDispatched =
      [[XCTestExpectation alloc] initWithDescription:@"Cancel dispatched"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [cancelDispatched fulfill];
  });
  [self waitForExpectations:@[ cancelDispatched ] timeout:1];
  XCTAssertEqual(fakeCentralManager.asyncCanceledPeripherals.count, 1u);

  XCTestExpectation *timeoutExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Queued L2CAP timed out"];
  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  XCTAssertNil(stream);
                  XCTAssertNotNil(error);
                  XCTAssertEqualObjects(error.domain, GNCBLEErrorDomain);
                  XCTAssertEqual(error.code, GNCBLEErrorBlePerperipheralConnectionTimeout);
                  [timeoutExpectation fulfill];
                }];
  [self waitForExpectations:@[ timeoutExpectation ] timeout:1];
  XCTAssertEqual(fakeCentralManager.connectRequests.count, 1u);
  // Already-in-flight cancel should not be issued a second time.
  XCTAssertEqual(fakeCentralManager.asyncCanceledPeripherals.count, 1u);

  // Delayed didDisconnectPeripheral: arrives after timeout and must not start a stale connect.
  [fakeCentralManager simulateCentralManagerDidDisconnectPeripheral:peripheral];
  XCTestExpectation *drainExpectation = [[XCTestExpectation alloc] initWithDescription:@"Drain"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [drainExpectation fulfill];
  });
  [self waitForExpectations:@[ drainExpectation ] timeout:1];
  XCTAssertEqual(fakeCentralManager.connectRequests.count, 1u);
}

- (void)testFailedGATTConnectClearsDisconnectionHandlerForSubsequentL2CAPSession {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  peripheral.channelInputStream = fakeStreams.inputStream;
  peripheral.channelOutputStream = fakeStreams.outputStream;

  fakeCentralManager.didFailToConnectPeripheralError = [NSError errorWithDomain:@"fake"
                                                                           code:0
                                                                       userInfo:nil];
  __block NSUInteger staleDisconnectCallCount = 0;
  XCTestExpectation *failedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"GATT connect failed"];
  [medium connectToGATTServerForPeripheral:peripheral
      disconnectionHandler:^{
        staleDisconnectCallCount++;
      }
      completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
        XCTAssertNil(client);
        XCTAssertNotNil(error);
        [failedExpectation fulfill];
      }];
  [self waitForExpectations:@[ failedExpectation ] timeout:1];

  NSDictionary *disconnectionHandlers = [medium valueForKey:@"_gattDisconnectionHandlers"];
  XCTAssertNil(disconnectionHandlers[peripheral.identifier]);

  // A subsequent L2CAP-only session that disconnects must not trigger the stale GATT handler.
  fakeCentralManager.didFailToConnectPeripheralError = nil;
  __block GNCBLEL2CAPStream *l2capStream = nil;
  XCTestExpectation *l2capOpened = [[XCTestExpectation alloc] initWithDescription:@"L2CAP opened"];
  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  XCTAssertNotNil(stream);
                  XCTAssertNil(error);
                  l2capStream = stream;
                  [l2capOpened fulfill];
                }];
  [self waitForExpectations:@[ l2capOpened ] timeout:1];

  [fakeCentralManager simulateCentralManagerDidDisconnectPeripheral:peripheral];
  XCTestExpectation *drainExpectation = [[XCTestExpectation alloc] initWithDescription:@"Drain"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [drainExpectation fulfill];
  });
  [self waitForExpectations:@[ drainExpectation ] timeout:1];
  XCTAssertEqual(staleDisconnectCallCount, 0u);

  [l2capStream tearDown];
  [fakeStreams tearDown];
}

- (void)testGATTConnectTimeoutClearsDisconnectionHandlerForSubsequentL2CAPSession {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableBleL2cap,
      true);
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  fakeCentralManager.shouldPendConnectPeripheral = YES;
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  [medium setPeripheralConnectTimeout:0.05];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  peripheral.channelInputStream = fakeStreams.inputStream;
  peripheral.channelOutputStream = fakeStreams.outputStream;

  __block NSUInteger staleDisconnectCallCount = 0;
  XCTestExpectation *timeoutExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"GATT connect timed out"];
  [medium connectToGATTServerForPeripheral:peripheral
      disconnectionHandler:^{
        staleDisconnectCallCount++;
      }
      completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
        XCTAssertNil(client);
        XCTAssertNotNil(error);
        XCTAssertEqualObjects(error.domain, GNCBLEErrorDomain);
        XCTAssertEqual(error.code, GNCBLEErrorBlePerperipheralConnectionTimeout);
        [timeoutExpectation fulfill];
      }];
  [self waitForExpectations:@[ timeoutExpectation ] timeout:1];

  NSDictionary *disconnectionHandlers = [medium valueForKey:@"_gattDisconnectionHandlers"];
  XCTAssertNil(disconnectionHandlers[peripheral.identifier]);
  XCTAssertEqual(staleDisconnectCallCount, 0u);

  // A subsequent L2CAP-only session that disconnects must not trigger the stale GATT handler.
  fakeCentralManager.shouldPendConnectPeripheral = NO;
  __block GNCBLEL2CAPStream *l2capStream = nil;
  XCTestExpectation *l2capOpened = [[XCTestExpectation alloc] initWithDescription:@"L2CAP opened"];
  [medium openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
                  XCTAssertNotNil(stream);
                  XCTAssertNil(error);
                  l2capStream = stream;
                  [l2capOpened fulfill];
                }];
  [self waitForExpectations:@[ l2capOpened ] timeout:1];

  [fakeCentralManager simulateCentralManagerDidDisconnectPeripheral:peripheral];
  XCTestExpectation *drainExpectation = [[XCTestExpectation alloc] initWithDescription:@"Drain"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [drainExpectation fulfill];
  });
  [self waitForExpectations:@[ drainExpectation ] timeout:1];
  XCTAssertEqual(staleDisconnectCallCount, 0u);

  [l2capStream tearDown];
  [fakeStreams tearDown];
}

- (void)testGATTDisconnectAfterRemoteDisconnectDoesNotCancelOrWedgeReconnect {
  GNCMediumAsyncCancelFakeCentralManager *fakeCentralManager =
      [[GNCMediumAsyncCancelFakeCentralManager alloc] init];
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                    peripheralManager:fakePeripheralManager
                                                                queue:dispatch_get_main_queue()];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];

  __block GNCBLEGATTClient *gattClient = nil;
  XCTestExpectation *connectedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"GATT connected"];
  XCTestExpectation *disconnectedExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"Remote disconnect handled"];
  [medium connectToGATTServerForPeripheral:peripheral
      disconnectionHandler:^{
        // Teardown often calls -disconnect from the disconnection handler or right after.
        [gattClient disconnect];
        [disconnectedExpectation fulfill];
      }
      completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
        XCTAssertNotNil(client);
        XCTAssertNil(error);
        gattClient = client;
        [connectedExpectation fulfill];
      }];
  [self waitForExpectations:@[ connectedExpectation ] timeout:3];
  XCTAssertEqual(fakeCentralManager.connectRequests.count, 1u);

  fakeCentralManager.shouldPendCancelPeripheralConnection = YES;
  [fakeCentralManager simulateCentralManagerDidDisconnectPeripheral:peripheral];
  [self waitForExpectations:@[ disconnectedExpectation ] timeout:3];

  dispatch_queue_t gattClientQueue = [gattClient valueForKey:@"_queue"];
  if (gattClientQueue) {
    dispatch_sync(gattClientQueue, ^{
                  });
  }
  XCTestExpectation *drainExpectation = [[XCTestExpectation alloc] initWithDescription:@"Drain"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [drainExpectation fulfill];
  });
  [self waitForExpectations:@[ drainExpectation ] timeout:3];

  XCTAssertEqual(fakeCentralManager.asyncCanceledPeripherals.count, 0u);
  NSSet<NSUUID *> *cancelingIdentifiers = [medium valueForKey:@"_cancelingPeripheralIdentifiers"];
  XCTAssertFalse([cancelingIdentifiers containsObject:peripheral.identifier]);

  XCTestExpectation *reconnectExpectation =
      [[XCTestExpectation alloc] initWithDescription:@"GATT reconnected"];
  [medium connectToGATTServerForPeripheral:peripheral
                      disconnectionHandler:nil
                         completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
                           XCTAssertNotNil(client);
                           XCTAssertNil(error);
                           [reconnectExpectation fulfill];
                         }];
  [self waitForExpectations:@[ reconnectExpectation ] timeout:3];
  XCTAssertEqual(fakeCentralManager.connectRequests.count, 2u);
}

- (void)testAdapterPoweredOffAndResettingClearsCancelingStateAndFailsPendingAttempts {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::kEnableBleL2cap,
      true);
  for (NSNumber *unavailableState in @[ @(CBManagerStatePoweredOff), @(CBManagerStateResetting) ]) {
    GNCMediumAsyncCancelFakeCentralManager *fakeCentralManager =
        [[GNCMediumAsyncCancelFakeCentralManager alloc] init];
    [fakeCentralManager simulateCentralManagerDidUpdateState:CBManagerStatePoweredOn];
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    [medium setPeripheralConnectTimeout:0.05];

    GNCFakePeripheral *peripheral1 = [[GNCFakePeripheral alloc] init];
    GNCFakePeripheral *peripheral2 = [[GNCFakePeripheral alloc] init];
    GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
        [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
    peripheral1.channelInputStream = fakeStreams.inputStream;
    peripheral1.channelOutputStream = fakeStreams.outputStream;

    __block GNCBLEGATTClient *gattClient1 = nil;
    __block NSUInteger disconnectCount1 = 0;
    XCTestExpectation *connected1 =
        [[XCTestExpectation alloc] initWithDescription:@"Peripheral 1 GATT connected"];
    [medium connectToGATTServerForPeripheral:peripheral1
        disconnectionHandler:^{
          disconnectCount1++;
        }
        completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
          XCTAssertNotNil(client);
          XCTAssertNil(error);
          gattClient1 = client;
          [connected1 fulfill];
        }];
    [self waitForExpectations:@[ connected1 ] timeout:3];

    // Put peripheral1 into _cancelingPeripheralIdentifiers and queue GATT + L2CAP behind it.
    fakeCentralManager.shouldPendCancelPeripheralConnection = YES;
    [gattClient1 disconnect];
    dispatch_queue_t gattClientQueue = [gattClient1 valueForKey:@"_queue"];
    if (gattClientQueue) {
      dispatch_sync(gattClientQueue, ^{
                    });
    }
    XCTestExpectation *cancelDispatched =
        [[XCTestExpectation alloc] initWithDescription:@"Cancel dispatched"];
    dispatch_async(dispatch_get_main_queue(), ^{
      [cancelDispatched fulfill];
    });
    [self waitForExpectations:@[ cancelDispatched ] timeout:3];

    XCTestExpectation *queuedGattFailed =
        [[XCTestExpectation alloc] initWithDescription:@"Queued GATT failed on power off"];
    [medium connectToGATTServerForPeripheral:peripheral1
                        disconnectionHandler:nil
                           completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
                             XCTAssertNil(client);
                             XCTAssertNotNil(error);
                             [queuedGattFailed fulfill];
                           }];

    XCTestExpectation *queuedL2capFailed =
        [[XCTestExpectation alloc] initWithDescription:@"Queued L2CAP failed on power off"];
    [medium openL2CAPChannelWithPSM:123
                         peripheral:peripheral1
                  completionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                      NSError *_Nullable error) {
                    XCTAssertNil(stream);
                    XCTAssertNotNil(error);
                    [queuedL2capFailed fulfill];
                  }];

    // Also leave peripheral2 with a pending connection attempt.
    fakeCentralManager.shouldPendConnectPeripheral = YES;
    XCTestExpectation *pendingGatt2Failed =
        [[XCTestExpectation alloc] initWithDescription:@"Pending GATT 2 failed on power off"];
    [medium connectToGATTServerForPeripheral:peripheral2
                        disconnectionHandler:nil
                           completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
                             XCTAssertNil(client);
                             XCTAssertNotNil(error);
                             [pendingGatt2Failed fulfill];
                           }];

    XCTestExpectation *queuedDrain =
        [[XCTestExpectation alloc] initWithDescription:@"Queued attempts registered"];
    dispatch_async(dispatch_get_main_queue(), ^{
      [queuedDrain fulfill];
    });
    [self waitForExpectations:@[ queuedDrain ] timeout:3];

    [fakeCentralManager
        simulateCentralManagerDidUpdateState:(CBManagerState)unavailableState.integerValue];
    [self waitForExpectations:@[ queuedGattFailed, queuedL2capFailed, pendingGatt2Failed ]
                      timeout:3];

    XCTAssertEqual(disconnectCount1, 1u);
    NSSet<NSUUID *> *cancelingIdentifiers = [medium valueForKey:@"_cancelingPeripheralIdentifiers"];
    NSDictionary *queuedPeripherals = [medium valueForKey:@"_queuedConnectPeripherals"];
    NSDictionary *queuedGATT = [medium valueForKey:@"_queuedGATTConnectionAttempts"];
    XCTAssertEqual(cancelingIdentifiers.count, 0u);
    XCTAssertEqual(queuedPeripherals.count, 0u);
    XCTAssertEqual(queuedGATT.count, 0u);

    // Wait past the 0.05s connect timeout while powered off; stale timeouts must not re-add to
    // _cancelingPeripheralIdentifiers or invoke cancelPeripheralConnection:.
    XCTestExpectation *postTimeoutDrain =
        [[XCTestExpectation alloc] initWithDescription:@"Post-timeout drain"];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     [postTimeoutDrain fulfill];
                   });
    [self waitForExpectations:@[ postTimeoutDrain ] timeout:3];
    XCTAssertEqual(cancelingIdentifiers.count, 0u);
    XCTAssertEqual(fakeCentralManager.asyncCanceledPeripherals.count, 1u);

    // Powering back on must allow immediate reconnection without waiting on stale cancels.
    fakeCentralManager.shouldPendConnectPeripheral = NO;
    [fakeCentralManager simulateCentralManagerDidUpdateState:CBManagerStatePoweredOn];
    XCTestExpectation *reconnected =
        [[XCTestExpectation alloc] initWithDescription:@"Reconnected after power on"];
    [medium connectToGATTServerForPeripheral:peripheral1
                        disconnectionHandler:nil
                           completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
                             XCTAssertNotNil(client);
                             XCTAssertNil(error);
                             [reconnected fulfill];
                           }];
    [self waitForExpectations:@[ reconnected ] timeout:3];

    [fakeStreams tearDown];
  }
}

- (void)testCancelPeripheralConnectionSkipsDisconnectedCBPeripheralAndUnpoweredCBCentralManager {
  // Case 1: CBPeripheral already in CBPeripheralStateDisconnected must not be canceled or added
  // to _cancelingPeripheralIdentifiers when timeout fires.
  {
    GNCMediumAsyncCancelFakeCentralManager *fakeCentralManager =
        [[GNCMediumAsyncCancelFakeCentralManager alloc] init];
    [fakeCentralManager simulateCentralManagerDidUpdateState:CBManagerStatePoweredOn];
    fakeCentralManager.shouldPendConnectPeripheral = YES;
    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    [medium setPeripheralConnectTimeout:0.05];

    id mockPeripheral = OCMClassMock([CBPeripheral class]);
    NSUUID *identifier = [[NSUUID alloc] init];
    OCMStub([mockPeripheral identifier]).andReturn(identifier);
    OCMStub([(CBPeripheral *)mockPeripheral state]).andReturn(CBPeripheralStateDisconnected);

    XCTestExpectation *timeoutExpectation =
        [[XCTestExpectation alloc] initWithDescription:@"L2CAP timeout on disconnected peripheral"];
    [medium openL2CAPChannelWithPSM:123
                         peripheral:mockPeripheral
                  completionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                      NSError *_Nullable error) {
                    XCTAssertNil(stream);
                    XCTAssertNotNil(error);
                    [timeoutExpectation fulfill];
                  }];
    [self waitForExpectations:@[ timeoutExpectation ] timeout:3];

    NSSet<NSUUID *> *cancelingIdentifiers = [medium valueForKey:@"_cancelingPeripheralIdentifiers"];
    XCTAssertEqual(fakeCentralManager.asyncCanceledPeripherals.count, 0u);
    XCTAssertFalse([cancelingIdentifiers containsObject:identifier]);
    [mockPeripheral stopMocking];
  }

  // Case 2: CBCentralManager not in CBManagerStatePoweredOn must not cancel or add to
  // _cancelingPeripheralIdentifiers.
  {
    id mockCentralManager = OCMClassMock([CBCentralManager class]);
    OCMStub([(CBCentralManager *)mockCentralManager state]).andReturn(CBManagerStateUnknown);
    OCMReject([mockCentralManager cancelPeripheralConnection:OCMOCK_ANY]);

    GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:mockCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    [medium setPeripheralConnectTimeout:0.05];
    GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];

    XCTestExpectation *timeoutExpectation =
        [[XCTestExpectation alloc] initWithDescription:@"L2CAP timeout while central unpowered"];
    [medium openL2CAPChannelWithPSM:123
                         peripheral:peripheral
                  completionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                      NSError *_Nullable error) {
                    XCTAssertNil(stream);
                    XCTAssertNotNil(error);
                    [timeoutExpectation fulfill];
                  }];
    [self waitForExpectations:@[ timeoutExpectation ] timeout:3];

    NSSet<NSUUID *> *cancelingIdentifiers = [medium valueForKey:@"_cancelingPeripheralIdentifiers"];
    XCTAssertFalse([cancelingIdentifiers containsObject:peripheral.identifier]);
    OCMVerifyAll(mockCentralManager);
    [mockCentralManager stopMocking];
  }
}

- (void)testDeallocWithoutStopDoesNotRetainPendingTimeoutBlocks {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  fakeCentralManager.shouldPendConnectPeripheral = YES;
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];

  __weak GNCBLEMedium *weakMedium = nil;
  __weak NSDictionary *weakTimeoutBlocks = nil;
  @autoreleasepool {
    GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager
                                                      peripheralManager:fakePeripheralManager
                                                                  queue:dispatch_get_main_queue()];
    [medium setPeripheralConnectTimeout:10.0];
    weakMedium = medium;
    weakTimeoutBlocks = [medium valueForKey:@"_l2capConnectionTimeoutBlocks"];

    [medium openL2CAPChannelWithPSM:123
                         peripheral:peripheral
                  completionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                      NSError *_Nullable error){
                  }];

    XCTestExpectation *scheduledExpectation =
        [[XCTestExpectation alloc] initWithDescription:@"Timeout block scheduled"];
    dispatch_async(dispatch_get_main_queue(), ^{
      [scheduledExpectation fulfill];
    });
    [self waitForExpectations:@[ scheduledExpectation ] timeout:3];
    XCTAssertEqual(weakTimeoutBlocks.count, 1u);
  }

  XCTAssertNil(weakMedium);
  XCTAssertNil(weakTimeoutBlocks);
}

@end
