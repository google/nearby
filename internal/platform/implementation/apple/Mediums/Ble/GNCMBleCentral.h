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

#import <CoreBluetooth/CoreBluetooth.h>

#import "internal/platform/implementation/apple/Mediums/GNCMConnection.h"

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
 * is a discovered set of characteristics.
 */
typedef void (^GNCMGATTDiscoverResultHandler)(
    NSOrderedSet<CBCharacteristic *> *_Nullable characteristicValues);

/**
 * This handler is called by a discovering endpoint to request a connection with an an advertising
 * endpoint.
 */
typedef void (^GNCMBleConnectionRequester)(NSString *serviceID,
                                           GNCMConnectionHandler connectionHandler);

/**
 * This handler is called on a discoverer when a nearby advertising endpoint is discovered.
 * Call |requestConnection| to request a connection with the advertiser.
 */
typedef void (^GNCMBleRequestConnectionHandler)(GNCMBleConnectionRequester requestConnection);

/**
 * GNCMBleCentral discovers devices advertising the specified service UUID via BLE (using the
 * GNCMBlePeripheral class) and calls the specififed scanning result handler when one is found.
 *
 * This class is thread-safe. Any calls made to it (and to the objects/closures it passes back via
 * callbacks) can be made from any thread/queue. Callbacks made from this class are called on the
 * specified queue.
 */
@interface GNCMBleCentral : NSObject

- (instancetype)init NS_DESIGNATED_INITIALIZER;

/**
 * Starts scanning with service UUID.
 *
 * @param serviceUUID A string that uniquely identifies the scanning services to search for.
 * @param scanResultHandler The handler that is called when an endpoint advertising the service
 *                          UUID is discovered.
 * @param requestConnectionHandler The handler that is called when an endpoint is discovered.
 * @param callbackQueue The queue on which all callbacks are made.
 */
- (BOOL)startScanningWithServiceUUID:(NSString *)serviceUUID
                   scanResultHandler:(GNCMScanResultHandler)scanResultHandler
            requestConnectionHandler:(GNCMBleRequestConnectionHandler)requestConnectionHandler
                       callbackQueue:(nullable dispatch_queue_t)callbackQueue;

/**
 * Sets up a GATT connection.
 *
 * @param peripheralID A string that uniquely identifies the peripheral.
 * @param gattConnectionResultHandler The handler that is called when an endpoint is
 * connected.
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
