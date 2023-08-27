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

#import <CoreBluetooth/CoreBluetooth.h>
#import "XCTest/XCTest.h"
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTCharacteristic.h"

@interface GNCBLEGATTCharacteristicTest : XCTestCase
@end

@implementation GNCBLEGATTCharacteristicTest

- (void)testConvenienceInit {
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:@"00000000-0000-3000-8000-000000000000"];
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"0000FEF3-0000-1000-8000-00805F9B34FB"];
  CBCharacteristicProperties properties = CBCharacteristicPropertyRead;
  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:characteristicUUID
                                         serviceUUID:serviceUUID
                                          properties:properties];
  XCTAssertEqualObjects(characteristic.characteristicUUID, characteristicUUID);
  XCTAssertEqualObjects(characteristic.serviceUUID, serviceUUID);
  XCTAssertEqual(characteristic.permissions, 0);
  XCTAssertEqual(characteristic.properties, properties);
}

- (void)testInit {
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:@"00000000-0000-3000-8000-000000000000"];
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"0000FEF3-0000-1000-8000-00805F9B34FB"];
  CBAttributePermissions permissions = CBAttributePermissionsReadable;
  CBCharacteristicProperties properties = CBCharacteristicPropertyRead;
  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:characteristicUUID
                                         serviceUUID:serviceUUID
                                         permissions:permissions
                                          properties:properties];
  XCTAssertEqualObjects(characteristic.characteristicUUID, characteristicUUID);
  XCTAssertEqualObjects(characteristic.serviceUUID, serviceUUID);
  XCTAssertEqual(characteristic.permissions, permissions);
  XCTAssertEqual(characteristic.properties, properties);
}

@end
