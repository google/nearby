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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTCharacteristic.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@implementation GNCBLEGATTCharacteristic

- (instancetype)initWithUUID:(CBUUID *)characteristicUUID
                 serviceUUID:(CBUUID *)serviceUUID
                  properties:(CBCharacteristicProperties)properties {
  return [self initWithUUID:characteristicUUID
                serviceUUID:serviceUUID
                permissions:0
                 properties:properties];
}

- (instancetype)initWithUUID:(CBUUID *)characteristicUUID
                 serviceUUID:(CBUUID *)serviceUUID
                 permissions:(CBAttributePermissions)permissions
                  properties:(CBCharacteristicProperties)properties {
  self = [super init];
  if (self) {
    _characteristicUUID = [characteristicUUID copy];
    _serviceUUID = [serviceUUID copy];
    _permissions = permissions;
    _properties = properties;
  }
  return self;
}

@end

NS_ASSUME_NONNULL_END
