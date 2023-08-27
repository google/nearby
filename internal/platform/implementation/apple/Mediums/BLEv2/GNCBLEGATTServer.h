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
 * A block to be invoked when a call to
 * @c createCharacteristicWithServiceID:characteristicUUID:permissions:properties:completionHandler:
 * has completed.
 *
 * @param characteristic The created characteristic, or @c nil if an error occurred.
 * @param error The cause of the failure, or @c nil if no error occurred.
 */
typedef void (^GNCCreateCharacteristicCompletionHandler)(
    GNCBLEGATTCharacteristic *_Nullable characteristic, NSError *_Nullable error);

/**
 * A block to be invoked when a call to @c updateCharacteristic:value:completionHandler: has
 * completed.
 *
 * @param error The cause of the failure, or @c nil if no error occurred.
 */
typedef void (^GNCUpdateCharacteristicCompletionHandler)(NSError *_Nullable error);

/**
 * A block to be invoked when a call to @c startAdvertisingData:completionHandler: has completed.
 *
 * @param error The cause of the failure, or @c nil if no error occurred.
 */
typedef void (^GNCStartAdvertisingCompletionHandler)(NSError *_Nullable error);

/**
 * A block to be invoked when a call to @c stopAdvertisingWithcompletionHandler: has completed.
 *
 * @param error The cause of the failure, or @c nil if no error occurred.
 */
typedef void (^GNCStopAdvertisingCompletionHandler)(NSError *_Nullable error);

/**
 * An object that manages and advertises GATT characteritics.
 *
 * @note The public APIs of this class are thread safe.
 */
@interface GNCBLEGATTServer : NSObject

/**
 * Creates a characteristic and adds it to the GATT server.
 *
 * Characteristics of the same service UUID will cause the service to be unpublished and republished
 * with the new characteristic appended.
 *
 * @param serviceUUID A 128-bit UUID that identifies the service that the characteristic belongs to.
 * @param characteristicUUID A 128-bit UUID that identifies the characteristic.
 * @param permissions The permissions of the characteristic value.
 * @param properties The properties of the characteristic.
 * @param completionHandler Called on a private queue with the characteristic if successfully
 *                          created or an error if one has occured.
 */
- (void)createCharacteristicWithServiceID:(CBUUID *)serviceUUID
                       characteristicUUID:(CBUUID *)characteristicUUID
                              permissions:(CBAttributePermissions)permissions
                               properties:(CBCharacteristicProperties)properties
                        completionHandler:
                            (nullable GNCCreateCharacteristicCompletionHandler)completionHandler;

/**
 * Updates a local characteristic with the provided value.
 *
 * @param characteristic The characteristic to update.
 * @param value The new value for the characteristic.
 * @param completionHandler Called on a private queue with @c nil if successfully updated or an
 *                          error if one has occured.
 */
- (void)updateCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                       value:(nullable NSData *)value
           completionHandler:(nullable GNCUpdateCharacteristicCompletionHandler)completionHandler;

/** Removes all published services from the local GATT database. */
- (void)stop;

/**
 * Starts advertising service data in a way that is supported by CoreBluetooth.
 *
 * Since CoreBluetooth doesn't support setting the @c CBAdvertisementDataServiceDataKey key, the
 * service list is advertised using @c CBAdvertisementDataServiceUUIDsKey and the associated data is
 * advertised using @c CBAdvertisementDataLocalNameKey. Since @c CBAdvertisementDataLocalNameKey
 * does not support binary data, the value is base64 encoded and truncated if the resulting value is
 * longer than 22 bytes. This also means we can only support advertising a single service.
 *
 * @param serviceData A dictionary that contains service-specific advertisement data.
 * @param completionHandler Called on a private queue with @c nil if successfully started
 *                          advertising or an error if one has occured.
 */
- (void)startAdvertisingData:(NSDictionary<CBUUID *, NSData *> *)serviceData
           completionHandler:(nullable GNCStartAdvertisingCompletionHandler)completionHandler;

/**
 * Stops advertising all service data.
 *
 * @param completionHandler Called on a private queue with @c nil if successfully stopped
 *                          advertising or an error if one has occured.
 */
- (void)stopAdvertisingWithCompletionHandler:
    (nullable GNCStopAdvertisingCompletionHandler)completionHandler;

@end

NS_ASSUME_NONNULL_END
