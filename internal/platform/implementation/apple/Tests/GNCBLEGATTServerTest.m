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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTCharacteristic.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTServer.h"
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

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  GNCBLEGATTCharacteristic *characteristic =
      [gattServer createCharacteristicWithServiceID:serviceUUID
                                 characteristicUUID:characteristicUUID
                                        permissions:CBAttributePermissionsReadable
                                         properties:CBCharacteristicPropertyRead];

  XCTAssertNotNil(characteristic);
  XCTAssertEqual(fakePeripheralManager.services.count, 1);
  XCTAssertEqual(fakePeripheralManager.services[0].characteristics.count, 1);
}

- (void)testCreateMultipleCharacteristicsForOneService {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID1 = [CBUUID UUIDWithString:kCharacteristicUUID1];
  CBUUID *characteristicUUID2 = [CBUUID UUIDWithString:kCharacteristicUUID2];
  GNCBLEGATTCharacteristic *characteristic1 =
      [gattServer createCharacteristicWithServiceID:serviceUUID
                                 characteristicUUID:characteristicUUID1
                                        permissions:CBAttributePermissionsReadable
                                         properties:CBCharacteristicPropertyRead];
  GNCBLEGATTCharacteristic *characteristic2 =
      [gattServer createCharacteristicWithServiceID:serviceUUID
                                 characteristicUUID:characteristicUUID2
                                        permissions:CBAttributePermissionsReadable
                                         properties:CBCharacteristicPropertyRead];

  XCTAssertNotNil(characteristic1);
  XCTAssertNotNil(characteristic2);
  XCTAssertEqual(fakePeripheralManager.services.count, 1);
  XCTAssertEqual(fakePeripheralManager.services[0].characteristics.count, 2);
}

- (void)testCreateCharacteristicNotPoweredOn {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  GNCBLEGATTCharacteristic *characteristic =
      [gattServer createCharacteristicWithServiceID:serviceUUID
                                 characteristicUUID:characteristicUUID
                                        permissions:CBAttributePermissionsReadable
                                         properties:CBCharacteristicPropertyRead];

  XCTAssertNil(characteristic);
}

- (void)testCreateCharacteristicServiceFailure {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  fakePeripheralManager.didAddServiceError = [NSError errorWithDomain:@"fake" code:0 userInfo:nil];
  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  GNCBLEGATTCharacteristic *characteristic =
      [gattServer createCharacteristicWithServiceID:serviceUUID
                                 characteristicUUID:characteristicUUID
                                        permissions:CBAttributePermissionsReadable
                                         properties:CBCharacteristicPropertyRead];

  XCTAssertNil(characteristic);
}

#pragma mark - Update Characteristic

- (void)testUpdateCharacteristic {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  GNCBLEGATTCharacteristic *characteristic =
      [gattServer createCharacteristicWithServiceID:serviceUUID
                                 characteristicUUID:characteristicUUID
                                        permissions:CBAttributePermissionsReadable
                                         properties:CBCharacteristicPropertyRead];

  BOOL success = [gattServer updateCharacteristic:characteristic value:[NSData data]];

  XCTAssertTrue(success);
  XCTAssertEqual(fakePeripheralManager.services.count, 1);
  XCTAssertEqual(fakePeripheralManager.services[0].characteristics.count, 1);
}

- (void)testUpdateCharacteristicInvalidService {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:characteristicUUID serviceUUID:serviceUUID];

  BOOL success = [gattServer updateCharacteristic:characteristic value:[NSData data]];

  XCTAssertFalse(success);
}

- (void)testUpdateCharacteristicInvalidCharacteristic {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID1 = [CBUUID UUIDWithString:kCharacteristicUUID1];
  CBUUID *characteristicUUID2 = [CBUUID UUIDWithString:kCharacteristicUUID2];
  [gattServer createCharacteristicWithServiceID:serviceUUID
                             characteristicUUID:characteristicUUID1
                                    permissions:CBAttributePermissionsReadable
                                     properties:CBCharacteristicPropertyRead];

  GNCBLEGATTCharacteristic *characteristic2 =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:characteristicUUID2 serviceUUID:serviceUUID];

  BOOL success = [gattServer updateCharacteristic:characteristic2 value:[NSData data]];

  XCTAssertFalse(success);
}

- (void)testUpdateCharacteristicNotPoweredOn {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  GNCBLEGATTCharacteristic *characteristic =
      [gattServer createCharacteristicWithServiceID:serviceUUID
                                 characteristicUUID:characteristicUUID
                                        permissions:CBAttributePermissionsReadable
                                         properties:CBCharacteristicPropertyRead];

  BOOL success = [gattServer updateCharacteristic:characteristic value:[NSData data]];

  XCTAssertFalse(success);
}

#pragma mark - Read Request

- (void)testReadRequest {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  GNCBLEGATTCharacteristic *characteristic =
      [gattServer createCharacteristicWithServiceID:serviceUUID
                                 characteristicUUID:characteristicUUID
                                        permissions:CBAttributePermissionsReadable
                                         properties:CBCharacteristicPropertyRead];

  [gattServer updateCharacteristic:characteristic value:[NSData data]];

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

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  GNCBLEGATTCharacteristic *characteristic =
      [gattServer createCharacteristicWithServiceID:serviceUUID
                                 characteristicUUID:characteristicUUID
                                        permissions:CBAttributePermissionsReadable
                                         properties:CBCharacteristicPropertyRead];

  [gattServer updateCharacteristic:characteristic value:[NSData data]];

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

  CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID1];
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID1];
  GNCBLEGATTCharacteristic *characteristic =
      [gattServer createCharacteristicWithServiceID:serviceUUID
                                 characteristicUUID:characteristicUUID
                                        permissions:CBAttributePermissionsReadable
                                         properties:CBCharacteristicPropertyRead];

  [gattServer updateCharacteristic:characteristic value:[NSData data]];

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
  BOOL success = [gattServer startAdvertisingData:@{}];

  XCTAssertTrue(success);
  XCTAssertTrue(fakePeripheralManager.isAdvertising);
  NSDictionary<NSString *, id> *data = fakePeripheralManager.advertisementData;
  XCTAssertEqual(data, nil);
}

- (void)testStartAdvertisingEmptyServiceData {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];
  BOOL success = [gattServer startAdvertisingData:@{
    [CBUUID UUIDWithString:@"FEF3"] : [NSData data],
  }];

  XCTAssertTrue(success);
  XCTAssertTrue(fakePeripheralManager.isAdvertising);
  NSDictionary<NSString *, id> *data = fakePeripheralManager.advertisementData;
  XCTAssertEqualObjects(data[CBAdvertisementDataLocalNameKey], @"");
  XCTAssertEqualObjects(data[CBAdvertisementDataServiceUUIDsKey][0],
                        [CBUUID UUIDWithString:@"FEF3"]);
}

- (void)testStartAdvertisingShortServiceData {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];
  BOOL success = [gattServer startAdvertisingData:@{
    [CBUUID UUIDWithString:@"FEF3"] : [@"0123" dataUsingEncoding:NSUTF8StringEncoding],
  }];

  XCTAssertTrue(success);
  XCTAssertTrue(fakePeripheralManager.isAdvertising);
  NSDictionary<NSString *, id> *data = fakePeripheralManager.advertisementData;
  XCTAssertEqualObjects(data[CBAdvertisementDataLocalNameKey], @"MDEyMw");
  XCTAssertEqualObjects(data[CBAdvertisementDataServiceUUIDsKey][0],
                        [CBUUID UUIDWithString:@"FEF3"]);
}

- (void)testStartAdvertising22ByteServiceData {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];
  BOOL success = [gattServer startAdvertisingData:@{
    [CBUUID UUIDWithString:@"FEF3"] : [@"0123456789012345" dataUsingEncoding:NSUTF8StringEncoding],
  }];

  XCTAssertTrue(success);
  XCTAssertTrue(fakePeripheralManager.isAdvertising);
  NSDictionary<NSString *, id> *data = fakePeripheralManager.advertisementData;
  XCTAssertEqualObjects(data[CBAdvertisementDataLocalNameKey], @"MDEyMzQ1Njc4OTAxMjM0NQ");
  XCTAssertEqualObjects(data[CBAdvertisementDataServiceUUIDsKey][0],
                        [CBUUID UUIDWithString:@"FEF3"]);
}

- (void)testStartAdvertisingLongServiceData {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];
  BOOL success = [gattServer startAdvertisingData:@{
    [CBUUID UUIDWithString:@"FEF3"] :
        [@"012345678901234567890123456789" dataUsingEncoding:NSUTF8StringEncoding],
  }];

  XCTAssertTrue(success);
  XCTAssertTrue(fakePeripheralManager.isAdvertising);
  NSDictionary<NSString *, id> *data = fakePeripheralManager.advertisementData;
  XCTAssertEqualObjects(data[CBAdvertisementDataLocalNameKey], @"MDEyMzQ1Njc4OTAxMjM0NT");
  XCTAssertEqualObjects(data[CBAdvertisementDataServiceUUIDsKey][0],
                        [CBUUID UUIDWithString:@"FEF3"]);
}

- (void)testStartAdvertisingWithEmojiServiceData {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];
  BOOL success = [gattServer startAdvertisingData:@{
    [CBUUID UUIDWithString:@"FEF3"] : [@"😁❤️🤡" dataUsingEncoding:NSUTF8StringEncoding],
  }];

  XCTAssertTrue(success);
  XCTAssertTrue(fakePeripheralManager.isAdvertising);
  NSDictionary<NSString *, id> *data = fakePeripheralManager.advertisementData;
  XCTAssertEqualObjects(data[CBAdvertisementDataLocalNameKey], @"8J-YgeKdpO-4j_CfpKE");
  XCTAssertEqualObjects(data[CBAdvertisementDataServiceUUIDsKey][0],
                        [CBUUID UUIDWithString:@"FEF3"]);
}

- (void)testStartAdvertisingMultipleServices {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];
  BOOL success = [gattServer startAdvertisingData:@{
    [CBUUID UUIDWithString:@"FEF3"] :
        [@"012345678901234567890123456789" dataUsingEncoding:NSUTF8StringEncoding],
    [CBUUID UUIDWithString:@"FEF4"] :
        [@"012345678901234567890123456789" dataUsingEncoding:NSUTF8StringEncoding],
  }];

  XCTAssertFalse(success);
  XCTAssertFalse(fakePeripheralManager.isAdvertising);
}

- (void)testStartAdvertisingNotPoweredOn {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  BOOL success = [gattServer startAdvertisingData:@{}];

  XCTAssertFalse(success);
  XCTAssertFalse(fakePeripheralManager.isAdvertising);
}

- (void)testStartAdvertisingStartFailure {
  GNCFakePeripheralManager *fakePeripheralManager = [[GNCFakePeripheralManager alloc] init];

  GNCBLEGATTServer *gattServer =
      [[GNCBLEGATTServer alloc] initWithPeripheralManager:fakePeripheralManager];

  fakePeripheralManager.didStartAdvertisingError = [NSError errorWithDomain:@"fake"
                                                                       code:0
                                                                   userInfo:nil];
  [fakePeripheralManager simulatePeripheralManagerDidUpdateState:CBManagerStatePoweredOn];
  BOOL success = [gattServer startAdvertisingData:@{}];

  XCTAssertFalse(success);
  XCTAssertFalse(fakePeripheralManager.isAdvertising);
}

@end
