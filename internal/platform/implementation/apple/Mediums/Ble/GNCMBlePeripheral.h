// Copyright 2022 Google LLC
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

#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/GNCMConnection.h"

@class CBCharacteristic;
@class CBUUID;

NS_ASSUME_NONNULL_BEGIN

/**
 * GNCMBlePeripheral advertises the specified service UUID via Ble for the purpose of being
 * discovered by a central using the GNCMBleCentral class.
 *
 * This class is thread-safe. Any calls made to it (and to the objects/closures it passes back via
 * callbacks) can be made from any thread/queue. Callbacks made from this class are called on the
 * specified queue.
 */
@interface GNCMBlePeripheral : NSObject

- (instancetype)init NS_DESIGNATED_INITIALIZER;

/**
 * Adds GATT CBService.
 *
 * @param serviceUUID A GATT service ID to advertise for.
 */
- (void)addCBServiceWithUUID:(CBUUID *)serviceUUID;

/**
 * Adds GATT CBCharacteristic.
 *
 * @param characteristic A characteristic CBUUID.
 */
- (void)addCharacteristic:(CBCharacteristic *)characteristic;

/**
 * Updates GATT CBCharacteristic with value.
 *
 * @param value The NSData to advertise.
 * @param characteristicUUID A characteristic CBUUID.
 */
- (void)updateValue:(NSData *)value forCharacteristic:(CBUUID *)characteristicUUID;

/**
 * Stops GATT server service.
 */
- (void)stopGATTService;

/**
 * Starts advertising with service UUID and advertisement data.
 *
 * @param serviceUUID A string that uniquely identifies the advertised service to search for.
 * @param advertisementData The data to advertise.
 * @param endpointconnectedHandler The handler that is called when a discoverer connects.
 * @param callbackQueue The queue on which all callbacks are made. If |callbackQueue| is not
 *                      provided, then the main queue is used in the function.
 */
- (BOOL)startAdvertisingWithServiceUUID:(NSString *)serviceUUID
                      advertisementData:(NSData *)advertisementData
               endpointConnectedHandler:(GNCMConnectionHandler)endpointConnectedHandler
                          callbackQueue:(nullable dispatch_queue_t)callbackQueue;

@end

NS_ASSUME_NONNULL_END
