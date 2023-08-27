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

@class GNCBLEGATTServer;
@class GNCBLEGATTClient;
@class GNCBLEGATTCharacteristic;

@protocol GNCPeripheral;

NS_ASSUME_NONNULL_BEGIN

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
 * A block to be invoked when a peripheral’s advertisement has been discovered.
 *
 * @note This block can be called numerous times.
 *
 * @param peripheral The discovered peripheral.
 * @param serviceData A dictionary that contains service-specific advertisement data. The keys
 *                    represent services and the values represent the service-specific data.
 */
typedef void (^GNCAdvertisementFoundHandler)(id<GNCPeripheral> peripheral,
                                             NSDictionary<CBUUID *, NSData *> *serviceData);

/**
 * A block to be invoked when a call to
 * @c startScanningForService:advertisementFoundHandler:completionHandler: has completed.
 *
 * @param error The cause of the failure, or @c nil if no error occurred.
 */
typedef void (^GNCStartScanningCompletionHandler)(NSError *_Nullable error);

/**
 * A block to be invoked when a call to @c stopScanningWithCompletionHandler: has completed.
 *
 * @param error The cause of the failure, or @c nil if no error occurred.
 */
typedef void (^GNCStopScanningCompletionHandler)(NSError *_Nullable error);

/**
 * A block to be invoked when a call to @c startGATTServerWithCompletionHandler: has completed.
 *
 * @param server The successfully started GATT server, or @c nil if an error occurred.
 * @param error The cause of the failure, or @c nil if no error occurred.
 */
typedef void (^GNCGATTServerCompletionHandler)(GNCBLEGATTServer *_Nullable server,
                                               NSError *_Nullable error);

/** A block to be invoked when a peripheral has disconnected. */
typedef void (^GNCGATTDisconnectionHandler)();

/**
 * A block to be invoked when a call to
 * @c connectToGATTServerForPeripheral:disconnectionHandler:completionHandler: has completed.
 *
 * @param client The interface to the remote peripheral’s GATT server, or @c nil if an error
 *               occurred.
 * @param error The cause of the failure, or @c nil if no error occurred.
 */
typedef void (^GNCGATTConnectionCompletionHandler)(GNCBLEGATTClient *_Nullable client,
                                                   NSError *_Nullable error);

/**
 * The main BLE medium used inside of Nearby. This serves as the entry point for all BLE and GATT
 * related operations.
 *
 * @note The public APIs of this class are thread safe.
 */
@interface GNCBLEMedium : NSObject

/** The hardware supports BOTH advertising extensions and extended scans. */
@property(nonatomic, readonly) BOOL supportsExtendedAdvertisements;

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

/**
 * Scans for peripherals that are advertising the specified service.
 *
 * @param serviceUUID The service UUID to scan for.
 * @param advertisementFoundHandler Called on a private queue when a peripheral has been discovered.
 * @param completionHandler Called on a private queue with @c nil if successfully started scanning
 *                          or an error if one has occured.
 */
- (void)startScanningForService:(CBUUID *)serviceUUID
      advertisementFoundHandler:(GNCAdvertisementFoundHandler)advertisementFoundHandler
              completionHandler:(nullable GNCStartScanningCompletionHandler)completionHandler;

/**
 * Stops scanning for peripherals.
 *
 * @param completionHandler Called on a private queue with @c nil if successfully stopped
 *                          scanning or an error if one has occured.
 */
- (void)stopScanningWithCompletionHandler:
    (nullable GNCStopScanningCompletionHandler)completionHandler;

/**
 * Starts a GATT server.
 *
 * @param completionHandler Called on a private queue with the GATT server if successfully started
 *                          or an error if one has occured.
 */
- (void)startGATTServerWithCompletionHandler:
    (nullable GNCGATTServerCompletionHandler)completionHandler;

/**
 * Connects to a peripheral’s GATT server.
 *
 * @param remotePeripheral The peripheral to which the central is attempting to connect.
 * @param disconnectionHandler Called on a private queue when the peripheral has been disconnected.
 * @param completionHandler Called on a private queue with a GATT client if successfully connected
 *                          or an error if one has occured.
 */
- (void)connectToGATTServerForPeripheral:(id<GNCPeripheral>)remotePeripheral
                    disconnectionHandler:(nullable GNCGATTDisconnectionHandler)disconnectionHandler
                       completionHandler:
                           (nullable GNCGATTConnectionCompletionHandler)completionHandler;

@end

NS_ASSUME_NONNULL_END
