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

@class CBUUID;

NS_ASSUME_NONNULL_BEGIN

/**
 * This handler is called on a discover when a nearby advertising endpoint is discovered.
 */
typedef void (^GNCMScanResultHandler)(NSString *peripheralID, NSData *serviceData);

/**
 * This handler is called on a discovery when a nearby advertising endpoint is connected.
 */
typedef void (^GNCMGATTConnectionResultHandler)(NSError *_Nullable error);

/**
 * This handler is called on a discovery when a nearby advertising endpoint is connected. The input
 * is a discovered map of characteristic values.
 */
typedef void (^GNCMGATTDiscoverResultHandler)(
    NSDictionary<CBUUID *, NSData *> *_Nullable characteristicValues);

/**
 * GNCMBleCentral discovers devices advertising the specified service UUID via BLE (using the
 * GNCMBlePeripheral class) and calls the specififed scanning result handler when one is found.
 *
 * This class is thread-safe. Any calls made to it (and to the objects/closures it passes back via
 * callbacks) can be made from any thread/queue. Callbacks made from this class are called on the
 * specified queue.
 */
@interface GNCMBleCentral : NSObject

- (instancetype)init;

/**
 * Starts scanning with service UUID.
 *
 * @param serviceUUID A string that uniquely identifies the scanning services to search for.
 * @param scanResultHandler The handler that is called when an endpoint advertising the service
 *                          UUID is discovered.
 */
- (BOOL)startScanningWithServiceUUID:(NSString *)serviceUUID
                   scanResultHandler:(GNCMScanResultHandler)scanResultHandler;

/**
 * Sets up a GATT connection.
 *
 * @param peripheralID A string that uniquely identifies the peripheral.
 * @param gattConnectionResultHandler The handler that is called when an endpoint is connected.
 */
- (void)connectGattServerWithPeripheralID:(NSString *)peripheralID
              gattConnectionResultHandler:
                  (GNCMGATTConnectionResultHandler)gattConnectionResultHandler;

/**
 * Discovers GATT service and its associated characteristics with values.
 *
 * @param serviceUUID A CBUUID for service to discover.
 * @param gattCharacteristics Array of CBUUID for characteristic to discover.
 * @param peripheralID A string that uniquely identifies the peripheral.
 * @param gattDiscoverResultHandler This handler is called on a discovery for a discovered map of
 *                                  characteristic values when a nearby advertising endpoint is
 *                                  connected.
 */
- (void)discoverGattService:(CBUUID *)serviceUUID
          gattCharacteristics:(NSArray<CBUUID *> *)characteristicUUIDs
                 peripheralID:(NSString *)peripheralID
    gattDiscoverResultHandler:(GNCMGATTDiscoverResultHandler)gattDiscoverResultHandler;

/**
 * Disconnects GATT connection.
 *
 * @param peripheralID A string that uniquely identifies the peripheral.
 */
- (void)disconnectGattServiceWithPeripheralID:(NSString *)peripheralID;

@end

NS_ASSUME_NONNULL_END
