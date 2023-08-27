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

@protocol GNCPeripheral;

NS_ASSUME_NONNULL_BEGIN

/**
 * A block to be invoked when a call to
 * @c discoverCharacteristicsWithUUIDs:serviceUUID:completionHandler: has completed.
 *
 * @param error On success, error will be @c nil. Otherwise, the error that occurred.
 */
typedef void (^GNCDiscoverCharacteristicsCompletionHandler)(NSError *_Nullable error);

/**
 * A block to be invoked when a call to @c characteristicWithUUID:serviceUUID:completionHandler: has
 * completed.
 *
 * @param characteristic The characteristic or @c nil if an error occurred.
 * @param error On success, error will be @c nil. Otherwise, the error that occurred.
 */
typedef void (^GNCGetCharacteristicCompletionHandler)(
    GNCBLEGATTCharacteristic *_Nullable characteristic, NSError *_Nullable error);

/**
 * A block to be invoked when a call to @c readValueForCharacteristic:completionHandler: has
 * completed.
 *
 * @param value The characteristic's value or @c nil if an error occurred.
 * @param error On success, error will be @c nil. Otherwise, the error that occurred.
 */
typedef void (^GNCReadCharacteristicValueCompletionHandler)(NSData *_Nullable value,
                                                            NSError *_Nullable error);

/**
 * An object that can be used to discover, explore, and interact with GATT services and
 * characteristics available on a remote peripheral.
 *
 * @note The public APIs of this class are thread safe.
 */
@interface GNCBLEGATTClient : NSObject

/** @remark init is not an available initializer. */
- (instancetype)init NS_UNAVAILABLE;

/**
 * Initializes the GATT client with a specified peripheral.
 *
 * @param peripheral The peripheral instance.
 */
- (instancetype)initWithPeripheral:(id<GNCPeripheral>)peripheral;

/**
 * Discovers the specified characteristics of a service.
 *
 * A successful call to this method means the characteristics can be retreived with
 * @c characteristicWithUUID:serviceUUID:completionHandler:.
 *
 * @param characteristics An array of 128-bit UUID objects of the characteristics that you are
 *                        interested in.
 * @param serviceUUID A 128-bit UUID that identifies the service that the characteristics belongs
 *                    to.
 * @param completionHandler Called on a private queue with @c nil if all characteristics have
 *                          successfully been discovered or else an error.
 */
- (void)discoverCharacteristicsWithUUIDs:(NSArray<CBUUID *> *)characteristicUUIDs
                             serviceUUID:(CBUUID *)serviceUUID
                       completionHandler:
                           (nullable GNCDiscoverCharacteristicsCompletionHandler)completionHandler;

/**
 * Retrieves a GATT characteristic.
 *
 * If you haven’t yet called the @c discoverCharacteristicsWithUUIDs:serviceUUID:completionHandler:
 * method to discover the characteristic, or if there was an error in doing so, this method will
 * complete with an error.
 *
 * @param characteristicUUID A 128-bit UUID that identifies the characteristic.
 * @param serviceUUID A 128-bit UUID that identifies the service that the characteristic belongs to.
 * @param completionHandler Called on a private queue with the characteristic if successful or an
 *                          error if one has occured.
 */
- (void)characteristicWithUUID:(CBUUID *)characteristicUUID
                   serviceUUID:(CBUUID *)serviceUUID
             completionHandler:(nullable GNCGetCharacteristicCompletionHandler)completionHandler;

/**
 * Reads the requested characteristic from the associated remote device.
 *
 * @param characteristic The characteristic whose value you want to read.
 * @param completionHandler Called on a private queue with the characteristics value if successful
 *                          or an error if one has occured.
 */
- (void)readValueForCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                 completionHandler:
                     (nullable GNCReadCharacteristicValueCompletionHandler)completionHandler;

@end

NS_ASSUME_NONNULL_END
