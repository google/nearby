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

NS_ASSUME_NONNULL_BEGIN

/** A container for GATT characteristic information. */
@interface GNCBLEGATTCharacteristic : NSObject

/** @remark init is not an available initializer. */
- (instancetype)init NS_UNAVAILABLE;

/**
 * Creates a container for GATT characteristic information with properties.
 *
 * @param characteristicUUID The UUID of the characteristic.
 * @param serviceUUID The UUID of the service.
 * @param properties The properties of the characteristic.
 */
- (instancetype)initWithUUID:(CBUUID *)characteristicUUID
                 serviceUUID:(CBUUID *)serviceUUID
                  properties:(CBCharacteristicProperties)properties;

/**
 * Creates a container for GATT characteristic information with permissions and properties.
 *
 * @param characteristicUUID A 128-bit UUID that identifies the characteristic.
 * @param serviceUUID A 128-bit UUID that identifies the service that the characteristic belongs to.
 * @param permissions The permissions of the characteristic value.
 * @param properties The properties of the characteristic.
 */
- (instancetype)initWithUUID:(CBUUID *)characteristicUUID
                 serviceUUID:(CBUUID *)serviceUUID
                 permissions:(CBAttributePermissions)permissions
                  properties:(CBCharacteristicProperties)properties NS_DESIGNATED_INITIALIZER;

/** The 128-bit UUID that identifies the characteristic. */
@property(nonatomic, readonly) CBUUID *characteristicUUID;

/** The 128-bit UUID that identifies the service that the characteristic belongs to. */
@property(nonatomic, readonly) CBUUID *serviceUUID;

/**
 * The permissions of the characteristic value.
 *
 * Characteristic permissions represent the read and write permissions for a characteristic's value.
 * These permissions can be a combination of multiple values.
 */
@property(nonatomic, readonly) CBAttributePermissions permissions;

/**
 * The properties of the characteristic.
 *
 * The properties of a characteristic determine the access to and use of the characteristic's value
 * and descriptors. These properties can be a combination of multiple values.
 */
@property(nonatomic, readonly) CBCharacteristicProperties properties;

@end

NS_ASSUME_NONNULL_END
