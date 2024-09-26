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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEMedium.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTClient.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTServer.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Tests/GNCBLEMedium+Testing.h"
#import "internal/platform/implementation/apple/Tests/GNCFakeCentralManager.h"
#import "internal/platform/implementation/apple/Tests/GNCFakePeripheral.h"

static NSString *const kServiceUUID = @"0000FEF3-0000-1000-8000-00805F9B34FB";

@interface GNCBLEMediumTest : XCTestCase
@end

@implementation GNCBLEMediumTest

#pragma mark - Supports Extended Advertisements

- (void)testSupportsExtendedAdvertisements {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];

  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];

  XCTAssertFalse([medium supportsExtendedAdvertisements]);
}

#pragma mark - Start Scanning

- (void)testStartScanning {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];
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

- (void)testAlreadyScanning {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];
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

- (void)testStartStopStartScanning {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];
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

#pragma mark - Decode Advertisement Data

- (void)testDecodeAndroidStyleAdvertisementData {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];
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

- (void)testDecodeAndroidStyleAdvertisementDataWithLocalName {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];
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

- (void)testDecodeAppleStyleAdvertisementData {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];
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

- (void)testDecodeInvalidAdvertisementData {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];
  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID];

  NSDictionary<NSString *, id> *data = @{
    CBAdvertisementDataLocalNameKey : @"!@#$",
    CBAdvertisementDataServiceUUIDsKey : @[ serviceUUID ],
  };
  NSDictionary<CBUUID *, NSData *> *actual = [medium decodeAdvertisementData:data];

  XCTAssertEqualObjects(@{}, actual);
}

- (void)testDecodeEmptyAdvertisementData {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];

  NSDictionary<CBUUID *, NSData *> *actual = [medium decodeAdvertisementData:@{}];

  XCTAssertEqualObjects(@{}, actual);
}

#pragma mark - Start GATT Server

- (void)testStartGATTServer {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];
  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Start GATT server."];

  [medium startGATTServerWithCompletionHandler:^(GNCBLEGATTServer *server, NSError *error) {
    XCTAssertNotNil(server);
    XCTAssertNil(error);
    [expectation fulfill];
  }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

#pragma mark - Start Advertising

- (void)testStartAdvertising {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];
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

- (void)testStopAdvertising {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];
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

#pragma mark - Connect

- (void)testSuccessfulConnect {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];
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

- (void)testFailedConnect {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];
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

- (void)testDisconnect {
  GNCFakeCentralManager *fakeCentralManager = [[GNCFakeCentralManager alloc] init];
  GNCBLEMedium *medium = [[GNCBLEMedium alloc] initWithCentralManager:fakeCentralManager queue:nil];
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

@end
