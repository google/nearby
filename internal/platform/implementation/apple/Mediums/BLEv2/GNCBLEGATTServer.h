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

@class GNCBLEGATTCharacteristic;

NS_ASSUME_NONNULL_BEGIN

/**
 * An object that manages and advertises GATT characteritics.
 *
 * @note The public APIs of this class are NOT thread safe. All methods in this class should be
 *       invoked from the same thread or serially.
 */
@interface GNCBLEGATTServer : NSObject

/**
 * Creates a characteristic and adds it to the GATT server.
 *
 * Characteristics of the same service UUID will cause the service to be unpublished and republished
 * with the new characteristic appended.
 *
 * This method blocks until the characteristic has successfully been added to the GATT server or an
 * error occurs.
 *
 * @param serviceUUID A 128-bit UUID that identifies the service that the characteristic belongs to.
 * @param characteristicUUID A 128-bit UUID that identifies the characteristic.
 * @param permissions The permissions of the characteristic value.
 * @param properties The properties of the characteristic.
 * @return Returns the characteristic or nil if an error has occured.
 */
- (nullable GNCBLEGATTCharacteristic *)
    createCharacteristicWithServiceID:(CBUUID *)serviceUUID
                   characteristicUUID:(CBUUID *)characteristicUUID
                          permissions:(CBAttributePermissions)permissions
                           properties:(CBCharacteristicProperties)properties;

/**
 * Updates a local characteristic with the provided value.
 *
 * @param characteristic The characteristic to update.
 * @param value The new value for the characteristic.
 * @return Returns @c YES if successfully updated or @c NO if an error has occured.
 */
- (BOOL)updateCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                       value:(nullable NSData *)value;

/** Removes all published services from the local GATT database. */
- (void)stop;

/**
 * Starts advertising service data in a way that is supported by CoreBluetooth.
 *
 * Since CoreBluetooth doesn't support setting the @c CBAdvertisementDataServiceDataKey key, the
 * service list is advertised using @c CBAdvertisementDataServiceUUIDsKey and the associated data is
 * advertised using @c CBAdvertisementDataLocalNameKey. Since @c CBAdvertisementDataLocalNameKey
 * does not support binary data, the value is base64 encoded and truncated if the resulting value is
 * longer than 22 bytes.
 *
 * This method blocks until the service data is being advertised or an error occurs.
 *
 * @param serviceData A dictionary that contains service-specific advertisement data.
 * @return Returns @c YES if successfully updated or @c NO if an error has occured.
 */
- (BOOL)startAdvertisingData:(NSDictionary<CBUUID *, NSData *> *)serviceData;

@end

NS_ASSUME_NONNULL_END
