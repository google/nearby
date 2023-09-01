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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTServer.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Tests/GNCBLEGATTServer+Testing.h"
#import "internal/platform/implementation/apple/Tests/GNCFakePeripheralManager.h"

static NSString *const kServiceUUID1 = @"0000FEF3-0000-1000-8000-00805F9B34FB";
static NSString *const kServiceUUID2 = @"0000FEF4-0000-1000-8000-00805F9B34FB";
static NSString *const kCharacteristicUUID1 = @"00000000-0000-3000-8000-000000000000";
static NSString *const kCharacteristicUUID2 = @"00000000-0000-3000-8000-000000000001";

@interface GNCBLEGATTServerTest : XCTestCase
@end

@implementation GNCBLEGATTServerTest

#pragma mark - Create Characteristic

- (void)testCreateCharacteristic {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Create characteristic."];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  [gattServer
      createCharacteristicWithServiceID:serviceUUID
                     characteristicUUID:characteristicUUID
                            permissions:CBAttributePermissionsReadable
                             properties:CBCharacteristicPropertyRead
                      completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                          NSError *error) {
                        XCTAssertNotNil(characteristic);
                        XCTAssertNil(error);
                        XCTAssertEqual(fakePeripheralManager.services.count, 1);
                        XCTAssertEqual(fakePeripheralManager.services[0].characteristics.count, 1);
                        [expectation fulfill];
                      }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testCreateMultipleCharacteristicsForOneService {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation1 =
      [[XCTestExpectation alloc] initWithDescription:@"Create characteristic 1."];
  XCTestExpectation *expectation2 =
      [[XCTestExpectation alloc] initWithDescription:@"Create characteristic 2."];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID1 = [CBUUID UUIDWithString:kCharacteristicUUID1];
  CBUUID *characteristicUUID2 = [CBUUID UUIDWithString:kCharacteristicUUID2];
  [gattServer createCharacteristicWithServiceID:serviceUUID
                             characteristicUUID:characteristicUUID1
                                    permissions:CBAttributePermissionsReadable
                                     properties:CBCharacteristicPropertyRead
                              completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                  NSError *error) {
                                XCTAssertNotNil(characteristic);
                                XCTAssertNil(error);
                                [expectation1 fulfill];
                              }];

  [gattServer createCharacteristicWithServiceID:serviceUUID
                             characteristicUUID:characteristicUUID2
                                    permissions:CBAttributePermissionsReadable
                                     properties:CBCharacteristicPropertyRead
                              completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                  NSError *error) {
                                XCTAssertNotNil(characteristic);
                                XCTAssertNil(error);
                                [expectation2 fulfill];
                              }];

  [self waitForExpectations:@[ expectation1, expectation2 ] timeout:3];
  XCTAssertEqual(fakePeripheralManager.services.count, 1);
  XCTAssertEqual(fakePeripheralManager.services[0].characteristics.count, 2);
}

- (void)testCreateDuplicateCharacteristics {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Create characteristic."];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID1 = [CBUUID UUIDWithString:kCharacteristicUUID1];
  [gattServer createCharacteristicWithServiceID:serviceUUID
                             characteristicUUID:characteristicUUID1
                                    permissions:CBAttributePermissionsReadable
                                     properties:CBCharacteristicPropertyRead
                              completionHandler:nil];

  [gattServer createCharacteristicWithServiceID:serviceUUID
                             characteristicUUID:characteristicUUID1
                                    permissions:CBAttributePermissionsReadable
                                     properties:CBCharacteristicPropertyRead
                              completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                  NSError *error) {
                                XCTAssertNil(characteristic);
                                XCTAssertNotNil(error);
                                [expectation fulfill];
                              }];

  [self waitForExpectations:@[ expectation ] timeout:3];
  XCTAssertEqual(fakePeripheralManager.services.count, 1);
  XCTAssertEqual(fakePeripheralManager.services[0].characteristics.count, 1);
}

- (void)testCreateDuplicatePendingCharacteristics {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Create characteristic."];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID1 = [CBUUID UUIDWithString:kCharacteristicUUID1];
  [gattServer createCharacteristicWithServiceID:serviceUUID
                             characteristicUUID:characteristicUUID1
                                    permissions:CBAttributePermissionsReadable
                                     properties:CBCharacteristicPropertyRead
                              completionHandler:nil];

  [gattServer createCharacteristicWithServiceID:serviceUUID
                             characteristicUUID:characteristicUUID1
                                    permissions:CBAttributePermissionsReadable
                                     properties:CBCharacteristicPropertyRead
                              completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                  NSError *error) {
                                XCTAssertNil(characteristic);
                                XCTAssertNotNil(error);
                                [expectation fulfill];
                              }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testCreateCharacteristicNotPoweredOn {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Create characteristic."];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  [gattServer createCharacteristicWithServiceID:serviceUUID
                             characteristicUUID:characteristicUUID
                                    permissions:CBAttributePermissionsReadable
                                     properties:CBCharacteristicPropertyRead
                              completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                  NSError *error) {
                                // The error occurs after completion, so its expected to fail
                                // silently.
                                XCTAssertNotNil(characteristic);
                                XCTAssertNil(error);
                                [expectation fulfill];
                              }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testCreateCharacteristicServiceFailure {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  fakePeripheralManager.didAddServiceError = [NSError errorWithDomain:@"fake" code:0 userInfo:nil];
  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Create characteristic."];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  [gattServer createCharacteristicWithServiceID:serviceUUID
                             characteristicUUID:characteristicUUID
                                    permissions:CBAttributePermissionsReadable
                                     properties:CBCharacteristicPropertyRead
                              completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                  NSError *error) {
                                // The error occurs after completion, so its expected to fail
                                // silently.
                                XCTAssertNotNil(characteristic);
                                XCTAssertNil(error);
                                [expectation fulfill];
                              }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

#pragma mark - Read Request

- (void)testReadRequest {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Create and update characteristic."];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  [gattServer createCharacteristicWithServiceID:serviceUUID
                             characteristicUUID:characteristicUUID
                                    permissions:CBAttributePermissionsReadable
                                     properties:CBCharacteristicPropertyRead
                              completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                  NSError *error) {
                                [gattServer updateCharacteristic:characteristic
                                                           value:[NSData data]
                                               completionHandler:^(NSError *error) {
                                                 [expectation fulfill];
                                               }];
                              }];

  [self waitForExpectations:@[ expectation ] timeout:3];

  [fakePeripheralManager
      simulatePeripheralManagerDidReceiveReadRequestForService:serviceUUID
                                                characteristic:characteristicUUID];

  [self waitForExpectations:@[ fakePeripheralManager.respondToRequestSuccessExpectation ]
                    timeout:0];
}

- (void)testReadRequestInvalidService {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Create and update characteristic."];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  [gattServer createCharacteristicWithServiceID:serviceUUID
                             characteristicUUID:characteristicUUID
                                    permissions:CBAttributePermissionsReadable
                                     properties:CBCharacteristicPropertyRead
                              completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                  NSError *error) {
                                [gattServer updateCharacteristic:characteristic
                                                           value:[NSData data]
                                               completionHandler:^(NSError *error) {
                                                 [expectation fulfill];
                                               }];
                              }];

  [self waitForExpectations:@[ expectation ] timeout:3];

  CBUUID *invalidServiceUUID = [CBUUID UUIDWithString:kServiceUUID2];

  [fakePeripheralManager
      simulatePeripheralManagerDidReceiveReadRequestForService:invalidServiceUUID
                                                characteristic:characteristicUUID];

  [self waitForExpectations:@[ fakePeripheralManager.respondToRequestErrorExpectation ] timeout:0];
}

- (void)testReadRequestInvalidCharacteristic {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Create and update characteristic."];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  [gattServer createCharacteristicWithServiceID:serviceUUID
                             characteristicUUID:characteristicUUID
                                    permissions:CBAttributePermissionsReadable
                                     properties:CBCharacteristicPropertyRead
                              completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                  NSError *error) {
                                [gattServer updateCharacteristic:characteristic
                                                           value:[NSData data]
                                               completionHandler:^(NSError *error) {
                                                 [expectation fulfill];
                                               }];
                              }];

  [self waitForExpectations:@[ expectation ] timeout:3];

  CBUUID *invalidCharacteristicUUID =
      [CBUUID UUIDWithString:kCharacteristicUUID2];

  [fakePeripheralManager
      simulatePeripheralManagerDidReceiveReadRequestForService:serviceUUID
                                                characteristic:invalidCharacteristicUUID];

  [self waitForExpectations:@[ fakePeripheralManager.respondToRequestErrorExpectation ] timeout:0];
}

#pragma mark - Stop

- (void)testStop {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [gattServer stop];

  XCTAssertEqual(fakePeripheralManager.services.count, 0);
}

#pragma mark - Start Advertising

- (void)testStartAdvertisingNoServiceData {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Start advertising."];

  [gattServer startAdvertisingData:@{}
                 completionHandler:^(NSError *error) {
                   XCTAssertNotNil(error);
                   XCTAssertFalse(fakePeripheralManager.isAdvertising);
                   [expectation fulfill];
                 }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testStartAdvertisingEmptyServiceData {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Start advertising."];

  [gattServer startAdvertisingData:@{[CBUUID UUIDWithString:@"FEF3"] : [NSData data]}
                 completionHandler:^(NSError *error) {
                   XCTAssertNil(error);
                   XCTAssertTrue(fakePeripheralManager.isAdvertising);
                   NSDictionary<NSString *, id> *data = fakePeripheralManager.advertisementData;
                   XCTAssertEqualObjects(data[CBAdvertisementDataLocalNameKey], @"");
                   XCTAssertEqualObjects(data[CBAdvertisementDataServiceUUIDsKey][0],
                                         [CBUUID UUIDWithString:@"FEF3"]);
                   [expectation fulfill];
                 }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testStartAdvertisingShortServiceData {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Start advertising."];

  [gattServer startAdvertisingData:@{
    [CBUUID UUIDWithString:@"FEF3"] : [@"0123" dataUsingEncoding:NSUTF8StringEncoding],
  }
                 completionHandler:^(NSError *error) {
                   XCTAssertNil(error);
                   XCTAssertTrue(fakePeripheralManager.isAdvertising);
                   NSDictionary<NSString *, id> *data = fakePeripheralManager.advertisementData;
                   XCTAssertEqualObjects(data[CBAdvertisementDataLocalNameKey], @"MDEyMw");
                   XCTAssertEqualObjects(data[CBAdvertisementDataServiceUUIDsKey][0],
                                         [CBUUID UUIDWithString:@"FEF3"]);
                   [expectation fulfill];
                 }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testStartAdvertising20ByteServiceData {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Start advertising."];

  [gattServer startAdvertisingData:@{
    [CBUUID UUIDWithString:@"FEF3"] : [@"012345678901234" dataUsingEncoding:NSUTF8StringEncoding],
  }
                 completionHandler:^(NSError *error) {
                   XCTAssertNil(error);
                   XCTAssertTrue(fakePeripheralManager.isAdvertising);
                   NSDictionary<NSString *, id> *data = fakePeripheralManager.advertisementData;
                   XCTAssertEqualObjects(data[CBAdvertisementDataLocalNameKey],
                                         @"MDEyMzQ1Njc4OTAxMjM0");
                   XCTAssertEqualObjects(data[CBAdvertisementDataServiceUUIDsKey][0],
                                         [CBUUID UUIDWithString:@"FEF3"]);
                   [expectation fulfill];
                 }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testStartAdvertisingLongServiceData {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Start advertising."];

  [gattServer
      startAdvertisingData:@{
        [CBUUID UUIDWithString:@"FEF3"] :
            [@"012345678901234567890123456789" dataUsingEncoding:NSUTF8StringEncoding],
      }
         completionHandler:^(NSError *error) {
           XCTAssertNil(error);
           XCTAssertTrue(fakePeripheralManager.isAdvertising);
           NSDictionary<NSString *, id> *data = fakePeripheralManager.advertisementData;
           XCTAssertEqualObjects(data[CBAdvertisementDataLocalNameKey], @"MDEyMzQ1Njc4OTAxMjM0");
           XCTAssertEqualObjects(data[CBAdvertisementDataServiceUUIDsKey][0],
                                 [CBUUID UUIDWithString:@"FEF3"]);
           [expectation fulfill];
         }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testStartAdvertisingWithEmojiServiceData {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Start advertising."];

  [gattServer
      startAdvertisingData:@{
        [CBUUID UUIDWithString:@"FEF3"] : [@"😁❤️🤡" dataUsingEncoding:NSUTF8StringEncoding],
      }
         completionHandler:^(NSError *error) {
           XCTAssertNil(error);
           XCTAssertTrue(fakePeripheralManager.isAdvertising);
           NSDictionary<NSString *, id> *data = fakePeripheralManager.advertisementData;
           XCTAssertEqualObjects(data[CBAdvertisementDataLocalNameKey], @"8J-YgeKdpO-4j_CfpKE");
           XCTAssertEqualObjects(data[CBAdvertisementDataServiceUUIDsKey][0],
                                 [CBUUID UUIDWithString:@"FEF3"]);
           [expectation fulfill];
         }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testStartAdvertisingMultipleServices {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Start advertising."];

  [gattServer startAdvertisingData:@{
    [CBUUID UUIDWithString:@"FEF3"] :
        [@"012345678901234567890123456789" dataUsingEncoding:NSUTF8StringEncoding],
    [CBUUID UUIDWithString:@"FEF4"] :
        [@"012345678901234567890123456789" dataUsingEncoding:NSUTF8StringEncoding],
  }
                 completionHandler:^(NSError *error) {
                   XCTAssertNotNil(error);
                   XCTAssertFalse(fakePeripheralManager.isAdvertising);
                   [expectation fulfill];
                 }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testStartAdvertisingNotPoweredOn {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Start advertising."];

  [gattServer startAdvertisingData:@{[CBUUID UUIDWithString:@"FEF3"] : [NSData data]}
                 completionHandler:^(NSError *error) {
                   // The error occurs after completion, so its expected to fail silently.
                   XCTAssertNil(error);
                   XCTAssertFalse(fakePeripheralManager.isAdvertising);
                   [expectation fulfill];
                 }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testStartAdvertisingStartFailure {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  fakePeripheralManager.didStartAdvertisingError = [NSError errorWithDomain:@"fake"
                                                                       code:0
                                                                   userInfo:nil];
  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Start advertising."];

  [gattServer startAdvertisingData:@{[CBUUID UUIDWithString:@"FEF3"] : [NSData data]}
                 completionHandler:^(NSError *error) {
                   // The error occurs after completion, so its expected to fail silently.
                   XCTAssertNil(error);
                   XCTAssertFalse(fakePeripheralManager.isAdvertising);
                   [expectation fulfill];
                 }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testStartAdvertisingAlreadyAdvertising {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  fakePeripheralManager.didStartAdvertisingError = [NSError errorWithDomain:@"fake"
                                                                       code:0
                                                                   userInfo:nil];
  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Start advertising."];

  [gattServer startAdvertisingData:@{[CBUUID UUIDWithString:@"FEF3"] : [NSData data]}
                 completionHandler:nil];

  [gattServer startAdvertisingData:@{[CBUUID UUIDWithString:@"FEF4"] : [NSData data]}
                 completionHandler:^(NSError *error) {
                   XCTAssertNotNil(error);
                   XCTAssertFalse(fakePeripheralManager.isAdvertising);
                   [expectation fulfill];
                 }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

- (void)testStartStopStartAdvertising {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  XCTestExpectation *expectation =
      [[XCTestExpectation alloc] initWithDescription:@"Start advertising."];

  [gattServer startAdvertisingData:@{[CBUUID UUIDWithString:@"FEF3"] : [NSData data]}
                 completionHandler:^(NSError *error) {
                   XCTAssertNil(error);
                   XCTAssertTrue(fakePeripheralManager.isAdvertising);
                   NSDictionary<NSString *, id> *data = fakePeripheralManager.advertisementData;
                   XCTAssertEqualObjects(data[CBAdvertisementDataLocalNameKey], @"");
                   XCTAssertEqualObjects(data[CBAdvertisementDataServiceUUIDsKey][0],
                                         [CBUUID UUIDWithString:@"FEF3"]);
                   [gattServer stopAdvertisingWithCompletionHandler:^(NSError *error) {
                     XCTAssertNil(error);
                     XCTAssertFalse(fakePeripheralManager.isAdvertising);
                     [gattServer
                         startAdvertisingData:@{[CBUUID UUIDWithString:@"FEF4"] : [NSData data]}
                            completionHandler:^(NSError *error) {
                              XCTAssertNil(error);
                              XCTAssertTrue(fakePeripheralManager.isAdvertising);
                              NSDictionary<NSString *, id> *data =
                                  fakePeripheralManager.advertisementData;
                              XCTAssertEqualObjects(data[CBAdvertisementDataLocalNameKey], @"");
                              XCTAssertEqualObjects(data[CBAdvertisementDataServiceUUIDsKey][0],
                                                    [CBUUID UUIDWithString:@"FEF4"]);
                              [expectation fulfill];
                            }];
                   }];
                 }];

  [self waitForExpectations:@[ expectation ] timeout:3];
}

@end
