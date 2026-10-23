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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTClient.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTServer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPClient+Testing.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEMedium+Testing.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakeCentralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheralManager.h"

#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "internal/flags/nearby_flags.h"

static NSString *const kServiceUUID = @"0000FEF3-0000-1000-8000-00805F9B34FB";

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

    // Stop advertising is fully covered with @c GNCBLEGATTServer tests. We are only testing stopping
    // without having started which tests the code paths relevant to @c GNCBLEMedium.
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
    [medium openL2CAPChannelWithPSM:123
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

@end
