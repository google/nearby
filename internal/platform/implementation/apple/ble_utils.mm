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

#import "internal/platform/implementation/apple/ble_utils.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include <string>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/utils.h"

namespace nearby {
namespace apple {

using GattCharacteristic = api::ble_v2::GattCharacteristic;
using Permission = api::ble_v2::GattCharacteristic::Permission;
using Property = api::ble_v2::GattCharacteristic::Property;
using WriteType = api::ble_v2::GattClient::WriteType;

/**
 * Checks if enum `a` contains enum `b`
 * 
 * For example, `Contains(Permission::kRead | Permission::kWrite, Permission::kRead)` should return
 * `YES`.
 */
template <typename T>
BOOL Contains(T a, T b) {
  return (a & b) == b;
}

BOOL Contains(unsigned long a, unsigned long b) { return Contains<unsigned long>(a, b); }

CBUUID *CBUUID16FromCPP(const Uuid &uuid) {
  return [CBUUID UUIDWithString:@(uuid.Get16BitAsString().c_str())];
}

CBUUID *CBUUID128FromCPP(const Uuid &uuid) {
  return [CBUUID UUIDWithString:@(std::string(uuid).c_str())];
}

CBCharacteristicWriteType CBCharacteristicWriteTypeFromCPP(WriteType wt) {
  switch (wt) {
    case WriteType::kWithResponse:
      return CBCharacteristicWriteWithResponse;
    case WriteType::kWithoutResponse:
      return CBCharacteristicWriteWithoutResponse;
  }
}

CBAttributePermissions CBAttributePermissionsFromCPP(Permission p) {
  CBAttributePermissions permissions = 0;
  if (Contains(p, Permission::kRead)) {
    permissions |= CBAttributePermissionsReadable;
  }
  if (Contains(p, Permission::kWrite)) {
    permissions |= CBAttributePermissionsWriteable;
  }
  return permissions;
}

CBCharacteristicProperties CBCharacteristicPropertiesFromCPP(Property p) {
  CBCharacteristicProperties properties = 0;
  if (Contains(p, Property::kRead)) {
    properties |= CBCharacteristicPropertyRead;
  }
  if (Contains(p, Property::kWrite)) {
    properties |= CBCharacteristicPropertyWrite;
  }
  if (Contains(p, Property::kIndicate)) {
    properties |= CBCharacteristicPropertyIndicate;
  }
  if (Contains(p, Property::kNotify)) {
    properties |= CBCharacteristicPropertyNotify;
  }
  return properties;
}

GNCBLEGATTCharacteristic *ObjCGATTCharacteristicFromCPP(const GattCharacteristic &c) {
  CBUUID *characteristicUUID = CBUUID128FromCPP(c.uuid);
  CBUUID *serviceUUID = CBUUID128FromCPP(c.service_uuid);
  CBAttributePermissions permissions = CBAttributePermissionsFromCPP(c.permission);
  CBCharacteristicProperties properties = CBCharacteristicPropertiesFromCPP(c.property);
  return [[GNCBLEGATTCharacteristic alloc] initWithUUID:characteristicUUID
                                            serviceUUID:serviceUUID
                                            permissions:permissions
                                             properties:properties];
}

NSMutableDictionary<CBUUID *, NSData *> *ObjCServiceDataFromCPP(
    const absl::flat_hash_map<Uuid, nearby::ByteArray> &sd) {
  NSMutableDictionary<CBUUID *, NSData *> *serviceData = [NSMutableDictionary dictionary];
  for (const auto &pair : sd) {
    CBUUID *key = CBUUID16FromCPP(pair.first);
    NSData *data = NSDataFromByteArray(pair.second);
    [serviceData setObject:data forKey:key];
  }
  return serviceData;
}

Uuid CPPUUIDFromObjC(CBUUID *uuid) {
  NSString *uuidString = uuid.UUIDString;
  if (uuidString.length == 4) {
    uuidString = [NSString stringWithFormat:@"0000%@-0000-1000-8000-00805F9B34FB", uuidString];
  }
  NSArray<NSString *> *components = [uuidString componentsSeparatedByString:@"-"];
  NSCAssert(components.count == 5, @"Invalid UUID string: %@", uuidString);
  uint64_t mostSignificantBits = strtoull(components[0].UTF8String, nil, 16);
  mostSignificantBits <<= 16;
  mostSignificantBits |= strtoull(components[1].UTF8String, nil, 16);
  mostSignificantBits <<= 16;
  mostSignificantBits |= strtoull(components[2].UTF8String, nil, 16);

  uint64_t leastSignificantBits = strtoull(components[3].UTF8String, nil, 16);
  leastSignificantBits <<= 48;
  leastSignificantBits |= strtoull(components[4].UTF8String, nil, 16);

  return Uuid(mostSignificantBits, leastSignificantBits);
}

Permission CPPGATTCharacteristicPermissionFromObjC(CBAttributePermissions p) {
  Permission permissions = Permission::kNone;
  if (Contains(p, CBAttributePermissionsReadable)) {
    permissions |= Permission::kRead;
  }
  if (Contains(p, CBAttributePermissionsWriteable)) {
    permissions |= Permission::kWrite;
  }
  return permissions;
}

Property CPPGATTCharacteristicPropertyFromObjC(CBCharacteristicProperties p) {
  Property property = Property::kNone;
  if (Contains(p, CBCharacteristicPropertyRead)) {
    property |= Property::kRead;
  }
  if (Contains(p, CBCharacteristicPropertyWrite)) {
    property |= Property::kWrite;
  }
  if (Contains(p, CBCharacteristicPropertyIndicate)) {
    property |= Property::kIndicate;
  }
  if (Contains(p, CBCharacteristicPropertyNotify)) {
    property |= Property::kNotify;
  }
  return property;
}

GattCharacteristic CPPGATTCharacteristicFromObjC(GNCBLEGATTCharacteristic *c) {
  GattCharacteristic characteristic;
  characteristic.uuid = CPPUUIDFromObjC(c.characteristicUUID);
  characteristic.service_uuid = CPPUUIDFromObjC(c.serviceUUID);
  characteristic.permission = CPPGATTCharacteristicPermissionFromObjC(c.permissions);
  characteristic.property = CPPGATTCharacteristicPropertyFromObjC(c.properties);
  return characteristic;
}

}  // namespace apple
}  // namespace nearby
