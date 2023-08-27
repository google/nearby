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
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/ble_utils.h"

#include "internal/platform/implementation/ble_v2.h"

using GattCharacteristic = ::nearby::api::ble_v2::GattCharacteristic;
using Property = ::nearby::api::ble_v2::GattCharacteristic::Property;
using Permission = ::nearby::api::ble_v2::GattCharacteristic::Permission;
using WriteType = ::nearby::api::ble_v2::GattClient::WriteType;
using Uuid = ::nearby::Uuid;
using ByteArray = ::nearby::ByteArray;

@interface GNCBLEUtilsTest : XCTestCase
@end

@implementation GNCBLEUtilsTest

#pragma mark - C++ to Objective-C

- (void)testCBUUID16FromCPP {
  CBUUID *expected = [CBUUID UUIDWithString:@"FEF3"];
  CBUUID *actual = ::nearby::apple::CBUUID16FromCPP(Uuid(0x0000FEF300001000, 0x800000805F9B34FB));
  XCTAssertEqualObjects(actual.UUIDString, expected.UUIDString);
}

- (void)testCBUUID128FromCPP {
  CBUUID *expected = [CBUUID UUIDWithString:@"0000FEF3-0000-1000-8000-00805F9B34FB"];
  CBUUID *actual = ::nearby::apple::CBUUID128FromCPP(Uuid(0x0000FEF300001000, 0x800000805F9B34FB));
  XCTAssertEqualObjects(actual.UUIDString, expected.UUIDString);
}

- (void)testCBCharacteristicWriteTypeFromCPPWithResponse {
  CBCharacteristicWriteType expected = CBCharacteristicWriteWithResponse;
  CBCharacteristicWriteType actual =
      ::nearby::apple::CBCharacteristicWriteTypeFromCPP(WriteType::kWithResponse);
  XCTAssertEqual(actual, expected);
}

- (void)testCBCharacteristicWriteTypeFromCPPWithoutResponse {
  CBCharacteristicWriteType expected = CBCharacteristicWriteWithoutResponse;
  CBCharacteristicWriteType actual =
      ::nearby::apple::CBCharacteristicWriteTypeFromCPP(WriteType::kWithoutResponse);
  XCTAssertEqual(actual, expected);
}

- (void)testCBCharacteristicPropertiesFromCPPEmptyProperty {
  CBCharacteristicProperties expected = 0;
  CBCharacteristicProperties actual =
      ::nearby::apple::CBCharacteristicPropertiesFromCPP(Property::kNone);
  XCTAssertEqual(actual, expected);
}

- (void)testCBCharacteristicPropertiesFromCPPSingleProperty {
  CBCharacteristicProperties expected = CBCharacteristicPropertyRead;
  CBCharacteristicProperties actual =
      ::nearby::apple::CBCharacteristicPropertiesFromCPP(Property::kRead);
  XCTAssertEqual(actual, expected);
}

- (void)testCBCharacteristicPropertiesFromCPPMultipleProperties {
  CBCharacteristicProperties expected = CBCharacteristicPropertyWrite |
                                        CBCharacteristicPropertyNotify |
                                        CBCharacteristicPropertyIndicate;
  CBCharacteristicProperties actual = ::nearby::apple::CBCharacteristicPropertiesFromCPP(
      Property::kWrite | Property::kNotify | Property::kIndicate);
  XCTAssertEqual(actual, expected);
}

- (void)testCBAttributePermissionsFromCPPEmptyPermission {
  CBAttributePermissions expected = 0;
  CBAttributePermissions actual = ::nearby::apple::CBAttributePermissionsFromCPP(Permission::kNone);
  XCTAssertEqual(actual, expected);
}

- (void)testCBAttributePermissionsFromCPPSinglePermission {
  CBAttributePermissions expected = CBAttributePermissionsReadable;
  CBAttributePermissions actual =
      ::nearby::apple::CBAttributePermissionsFromCPP(Permission::kRead);
  XCTAssertEqual(actual, expected);
}

- (void)testCBAttributePermissionsFromCPPMultiplePermissions {
  CBAttributePermissions expected =
      CBAttributePermissionsReadable | CBAttributePermissionsWriteable;
  CBAttributePermissions actual =
      ::nearby::apple::CBAttributePermissionsFromCPP(Permission::kRead | Permission::kWrite);
  XCTAssertEqual(actual, expected);
}

- (void)testObjCGATTCharacteristicFromCPP {
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:@"00000000-0000-3000-8000-000000000000"];
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"0000FEF3-0000-1000-8000-00805F9B34FB"];
  CBAttributePermissions permissions = CBAttributePermissionsReadable;
  CBCharacteristicProperties properties = CBCharacteristicPropertyRead;

  GattCharacteristic characteristic = {Uuid(0x0000000000003000, 0x8000000000000000),
                                       Uuid(0x0000FEF300001000, 0x800000805F9B34FB),
                                       Permission::kRead, Property::kRead};

  GNCBLEGATTCharacteristic *actual = ::nearby::apple::ObjCGATTCharacteristicFromCPP(characteristic);
  XCTAssertEqualObjects(actual.characteristicUUID, characteristicUUID);
  XCTAssertEqualObjects(actual.serviceUUID, serviceUUID);
  XCTAssertEqual(actual.permissions, permissions);
  XCTAssertEqual(actual.properties, properties);
}

- (void)testObjCServiceDataFromCpp {
  CBUUID *serviceUUID1 = [CBUUID UUIDWithString:@"0000FEF3-0000-1000-8000-00805F9B34FB"];
  CBUUID *serviceUUID2 = [CBUUID UUIDWithString:@"0000FEF4-0000-1000-8000-00805F9B34FB"];
  NSData *data1 = [@"one" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *data2 = [@"two" dataUsingEncoding:NSUTF8StringEncoding];

  absl::flat_hash_map<Uuid, ByteArray> service_data;
  service_data[Uuid(0x0000FEF300001000, 0x800000805F9B34FB)] = ByteArray("one");
  service_data[Uuid(0x0000FEF400001000, 0x800000805F9B34FB)] = ByteArray("two");

  NSMutableDictionary<CBUUID *, NSData *> *actual =
      ::nearby::apple::ObjCServiceDataFromCPP(service_data);
  XCTAssertEqualObjects(actual[serviceUUID1], data1);
  XCTAssertEqualObjects(actual[serviceUUID2], data2);
}

#pragma mark - Objective-C to C++

- (void)testCPPUUIDFromObjC16Bit {
  Uuid expected = Uuid(0x0000FEF300001000, 0x800000805F9B34FB);
  Uuid actual = ::nearby::apple::CPPUUIDFromObjC([CBUUID UUIDWithString:@"FEF3"]);
  XCTAssertEqualObjects(@(std::string(actual).c_str()), @(std::string(expected).c_str()));
}

- (void)testCPPUUIDFromObjC128Bit {
  Uuid expected = Uuid(0x0000FEF300001000, 0x800000805F9B34FB);
  Uuid actual = ::nearby::apple::CPPUUIDFromObjC(
      [CBUUID UUIDWithString:@"0000FEF3-0000-1000-8000-00805F9B34FB"]);
  XCTAssertEqualObjects(@(std::string(actual).c_str()), @(std::string(expected).c_str()));
}

- (void)testCPPGATTCharacteristicPropertyFromObjCEmptyProperty {
  Property expected = Property::kNone;
  Property actual = ::nearby::apple::CPPGATTCharacteristicPropertyFromObjC(0);
  XCTAssertEqual(actual, expected);
}

- (void)testCPPGATTCharacteristicPropertyFromObjCSingleProperty {
  Property expected = Property::kRead;
  Property actual =
      ::nearby::apple::CPPGATTCharacteristicPropertyFromObjC(CBCharacteristicPropertyRead);
  XCTAssertEqual(actual, expected);
}

- (void)testCPPGATTCharacteristicPropertyFromObjCMultipleProperties {
  Property expected = Property::kWrite | Property::kNotify | Property::kIndicate;
  Property actual = ::nearby::apple::CPPGATTCharacteristicPropertyFromObjC(
      CBCharacteristicPropertyWrite | CBCharacteristicPropertyNotify |
      CBCharacteristicPropertyIndicate);
  XCTAssertEqual(actual, expected);
}

- (void)testCPPGATTCharacteristicPermissionFromObjCEmptyPermission {
  Permission expected = Permission::kNone;
  Permission actual = ::nearby::apple::CPPGATTCharacteristicPermissionFromObjC(0);
  XCTAssertEqual(actual, expected);
}

- (void)testCPPGATTCharacteristicPermissionFromObjCSinglePermission {
  Permission expected = Permission::kRead;
  Permission actual =
      ::nearby::apple::CPPGATTCharacteristicPermissionFromObjC(CBAttributePermissionsReadable);
  XCTAssertEqual(actual, expected);
}

- (void)testCPPGATTCharacteristicPermissionFromObjCMultiplePermissions {
  Permission expected = Permission::kRead | Permission::kWrite;
  Permission actual = ::nearby::apple::CPPGATTCharacteristicPermissionFromObjC(
      CBAttributePermissionsReadable | CBAttributePermissionsWriteable);
  XCTAssertEqual(actual, expected);
}

- (void)testCPPGATTCharacteristicFromObjC {
  CBUUID *characteristicUUID = [CBUUID UUIDWithString:@"00000000-0000-3000-8000-000000000000"];
  CBUUID *serviceUUID = [CBUUID UUIDWithString:@"0000FEF3-0000-1000-8000-00805F9B34FB"];
  CBAttributePermissions permissions = CBAttributePermissionsReadable;
  CBCharacteristicProperties properties = CBCharacteristicPropertyRead;

  GNCBLEGATTCharacteristic *characteristic =
      [[GNCBLEGATTCharacteristic alloc] initWithUUID:characteristicUUID
                                         serviceUUID:serviceUUID
                                         permissions:permissions
                                          properties:properties];

  GattCharacteristic actual = ::nearby::apple::CPPGATTCharacteristicFromObjC(characteristic);
  XCTAssertEqual(actual.uuid, Uuid(0x0000000000003000, 0x8000000000000000));
  XCTAssertEqual(actual.service_uuid, Uuid(0x0000FEF300001000, 0x800000805F9B34FB));
  XCTAssertEqual(actual.permission, Permission::kRead);
  XCTAssertEqual(actual.property, Property::kRead);
}

@end
