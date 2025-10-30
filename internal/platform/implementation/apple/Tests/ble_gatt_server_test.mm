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

#include "internal/platform/implementation/apple/ble_gatt_server.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <XCTest/XCTest.h>

#include <string>
#include <vector>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTServer.h"
#import "internal/platform/implementation/apple/ble_utils.h"
#include "internal/platform/implementation/ble.h"

// Fake implementation for GNCBLEGATTServer
@interface GNCGattServerTestFakeBLEGATTServer : NSObject
@property(nonatomic) GNCBLEGATTCharacteristic *characteristicToReturn;
@property(nonatomic) NSError *errorToReturn;
@property(nonatomic) BOOL stopCalled;
@end

@implementation GNCGattServerTestFakeBLEGATTServer

- (void)createCharacteristicWithServiceID:(CBUUID *)serviceUUID
                       characteristicUUID:(CBUUID *)characteristicUUID
                              permissions:(CBAttributePermissions)permissions
                               properties:(CBCharacteristicProperties)properties
                        completionHandler:
                            (void (^)(GNCBLEGATTCharacteristic *_Nullable characteristic,
                                      NSError *_Nullable error))completionHandler {
  if (completionHandler) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      completionHandler(self.errorToReturn ? nil : self.characteristicToReturn, self.errorToReturn);
    });
  }
}

- (void)updateCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                       value:(NSData *)value
           completionHandler:(void (^)(NSError *_Nullable error))completionHandler {
  if (completionHandler) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      completionHandler(self.errorToReturn);
    });
  }
}

- (void)stop {
  self.stopCalled = YES;
}

@end

@interface GNCGattServerTest : XCTestCase
@end

@implementation GNCGattServerTest {
  nearby::apple::GattServer *_gattServer;
  GNCGattServerTestFakeBLEGATTServer *_fakeGNCBLEGATTServer;
}

- (void)setUp {
  [super setUp];
  _fakeGNCBLEGATTServer = [[GNCGattServerTestFakeBLEGATTServer alloc] init];
  _gattServer = new nearby::apple::GattServer((GNCBLEGATTServer *)_fakeGNCBLEGATTServer);
}

- (void)tearDown {
  delete _gattServer;
  [super tearDown];
}

- (void)testCreateCharacteristicSuccess {
  nearby::Uuid serviceUuid = nearby::Uuid([[CBUUID UUIDWithString:@"FEF3"] UUIDString].UTF8String);
  nearby::Uuid characteristicUuid =
      nearby::Uuid([[CBUUID UUIDWithString:@"B2B4"] UUIDString].UTF8String);
  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:[CBUUID UUIDWithString:@"B2B4"]
                                         serviceUUID:[CBUUID UUIDWithString:@"FEF3"]
                                         permissions:CBAttributePermissionsReadable
                                          properties:CBCharacteristicPropertyRead];
  _fakeGNCBLEGATTServer.characteristicToReturn = (GNCBLEGATTCharacteristic *)characteristic;

  std::optional<nearby::api::ble::GattCharacteristic> result = _gattServer->CreateCharacteristic(
      serviceUuid, characteristicUuid, nearby::api::ble::GattCharacteristic::Permission::kRead,
      nearby::api::ble::GattCharacteristic::Property::kRead);
  XCTAssertTrue(result.has_value());
}

- (void)testCreateCharacteristicFailure {
  nearby::Uuid serviceUuid = nearby::Uuid([[CBUUID UUIDWithString:@"FEF3"] UUIDString].UTF8String);
  nearby::Uuid characteristicUuid =
      nearby::Uuid([[CBUUID UUIDWithString:@"B2B4"] UUIDString].UTF8String);
  _fakeGNCBLEGATTServer.errorToReturn = [NSError errorWithDomain:@"test" code:1 userInfo:nil];

  std::optional<nearby::api::ble::GattCharacteristic> result = _gattServer->CreateCharacteristic(
      serviceUuid, characteristicUuid, nearby::api::ble::GattCharacteristic::Permission::kRead,
      nearby::api::ble::GattCharacteristic::Property::kRead);
  XCTAssertFalse(result.has_value());
}

- (void)testUpdateCharacteristicSuccess {
  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:[CBUUID UUIDWithString:@"B2B4"]
                                         serviceUUID:[CBUUID UUIDWithString:@"FEF3"]
                                         permissions:CBAttributePermissionsReadable
                                          properties:CBCharacteristicPropertyRead];
  nearby::api::ble::GattCharacteristic cppCharacteristic =
      nearby::apple::CPPGATTCharacteristicFromObjC((GNCBLEGATTCharacteristic *)characteristic);
  nearby::ByteArray value("test");

  BOOL result = _gattServer->UpdateCharacteristic(cppCharacteristic, value);
  XCTAssertTrue(result);
}

- (void)testUpdateCharacteristicFailure {
  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:[CBUUID UUIDWithString:@"B2B4"]
                                         serviceUUID:[CBUUID UUIDWithString:@"FEF3"]
                                         permissions:CBAttributePermissionsReadable
                                          properties:CBCharacteristicPropertyRead];
  nearby::api::ble::GattCharacteristic cppCharacteristic =
      nearby::apple::CPPGATTCharacteristicFromObjC((GNCBLEGATTCharacteristic *)characteristic);
  nearby::ByteArray value("test");
  _fakeGNCBLEGATTServer.errorToReturn = [NSError errorWithDomain:@"test" code:1 userInfo:nil];

  BOOL result = _gattServer->UpdateCharacteristic(cppCharacteristic, value);
  XCTAssertFalse(result);
}

- (void)testStop {
  _gattServer->Stop();
  XCTAssertTrue(_fakeGNCBLEGATTServer.stopCalled);
}

- (void)testNotifyCharacteristicChangedUnimplemented {
  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:[CBUUID UUIDWithString:@"B2B4"]
                                         serviceUUID:[CBUUID UUIDWithString:@"FEF3"]
                                         permissions:CBAttributePermissionsReadable
                                          properties:CBCharacteristicPropertyRead];
  nearby::api::ble::GattCharacteristic cppCharacteristic =
      nearby::apple::CPPGATTCharacteristicFromObjC((GNCBLEGATTCharacteristic *)characteristic);
  nearby::ByteArray value("test");

  absl::Status status = _gattServer->NotifyCharacteristicChanged(cppCharacteristic, false, value);
  XCTAssertEqual(status.code(), absl::StatusCode::kUnimplemented);
}

@end
