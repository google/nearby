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

#include "internal/platform/implementation/apple/ble_peripheral.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <XCTest/XCTest.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"

/** Fake @c GNCPeripheral for testing. */
@interface FakePeripheral : NSObject <GNCPeripheral>

@property(readwrite, copy) NSString *name;
@property(readwrite, nonatomic) NSUUID *identifier;
@property(nonatomic, readonly) CBPeripheralState state;
@property(nonatomic, weak) id<GNCPeripheralDelegate> peripheralDelegate;

- (instancetype)initWithIdentifier:(NSUUID *)identifier name:(NSString *)name;

@end

@implementation FakePeripheral

@synthesize peripheralDelegate = _peripheralDelegate;

- (instancetype)initWithIdentifier:(NSUUID *)identifier name:(NSString *)name {
  self = [super init];
  if (self) {
    _identifier = identifier;
    _name = [name copy];
    _state = CBPeripheralStateDisconnected;
  }
  return self;
}

// Implement other GNCPeripheral methods as no-ops or with default return values.
- (void)discoverServices:(nullable NSArray<CBUUID *> *)serviceUUIDs {
}

- (nullable NSArray<CBService *> *)services {
  return @[];
}

- (void)discoverCharacteristics:(nullable NSArray<CBUUID *> *)characteristicUUIDs
                     forService:(CBService *)service {
}

- (void)readValueForCharacteristic:(GNCBLEGATTCharacteristic *)characteristic {
}

- (void)writeValue:(NSData *)data
    forCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                 type:(CBCharacteristicWriteType)type {
}

- (BOOL)canSendWriteWithoutResponse {
  return YES;
}

- (void)setNotifyValue:(BOOL)enabled forCharacteristic:(GNCBLEGATTCharacteristic *)characteristic {
}

- (NSUInteger)maximumWriteLengthForType:(CBCharacteristicWriteType)type {
  return 20;
}

- (void)openL2CAPChannel:(CBL2CAPPSM)PSM {
}

@end

@interface GNCBLEPeripheralTest : XCTestCase
@end

@implementation GNCBLEPeripheralTest

- (void)testIsNull {
  nearby::apple::BlePeripheral blePeripheral{nil};
  XCTAssertNil(blePeripheral.GetPeripheral(), @"Peripheral should be nil");
}

- (void)testDefaultBlePeripheral {
  nearby::api::ble_v2::BlePeripheral &defaultPeripheral =
      nearby::apple::BlePeripheral::DefaultBlePeripheral();
  // 0xffffffffffffffff is the default value from
  // third_party/nearby/internal/platform/implementation/apple/ble_peripheral.mm when create static
  // default BlePeripheral.
  XCTAssertEqual(defaultPeripheral.GetUniqueId(), 0xffffffffffffffff);
}

- (void)testGetPeripheral {
  NSUUID *uuid = [NSUUID UUID];
  FakePeripheral *fakePeripheral = [[FakePeripheral alloc] initWithIdentifier:uuid
                                                                         name:@"TestDevice"];
  nearby::apple::BlePeripheral blePeripheral{fakePeripheral};
  XCTAssertEqualObjects(blePeripheral.GetPeripheral(), fakePeripheral);
}

@end
