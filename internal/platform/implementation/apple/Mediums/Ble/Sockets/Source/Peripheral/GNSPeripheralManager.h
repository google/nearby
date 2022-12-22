// Copyright 2020 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSUtils.h"

@class GNSPeripheralServiceManager;

/**
 * This class is in charge of the CBPeripheralManager. It owns the list of
 * GNSPeripheralServiceManager to advertise. This class dispatches events received from
 * the CBPeripheralManager.
 *
 *  * To create a GNSPeripheralManager instance:
 * GNSPeripheralManager *peripheralManager =
 *     [[GNSPeripheralManager alloc] initWithAdvertisedName:@"AdvertisedName"
 *                                        restoreIdentifier:@"BLEIdentifier"];
 *
 *  * To create a service:
 * GNSShouldAcceptSocketHandler shouldAcceptSocketHandler = ^(GNSSocket *socket) {
 *   socket.delegate = mySocketDelegate;
 *   return YES;
 * }
 * GNSPeripheralServiceManager *service =
 *     [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:serviceUUID
 *                                       shouldAcceptSocketHandler:shouldAcceptSocketHandler];
 * [peripheralManager addPeripheralServiceManager:service bleServiceAddedCompletion:handler];
 *
 *
 *  * When a socket is created, |shouldAcceptSocketHandler| is called. If the handler returns YES,
 * the socket delegate should be set, and the socket should be retained. The socket delegate
 * will be called as soon as the socket is connected. If the handler returns NO, the socket
 * should not be used.
 * See GNSSocket documentation to send and receive data.
 *
 * This class is not thread-safe.
 */
@interface GNSPeripheralManager : NSObject

@property(nonatomic, readonly, getter=isStarted) BOOL started;
@property(nonatomic, readonly) CBManagerState peripheralManagerState;
@property(nonatomic, copy) GNSPeripheralManagerStateHandler peripheralManagerStateHandler;

/**
 * Display name used in the BLE advertisement.
 *
 * Note that the change of the advertisement name will only be effective on the next change of
 * the advertised service UUIDs.
 */
@property(nonatomic, copy) NSString *advertisedName;

/**
 * Creates a CBPeripheralManager instance and sets itself as the delegate.  Pass nil to
 * |advertisedName| to avoid setting the BLE advertised name; this is useful for avoiding a name
 * collision with clients that use the name in a custom discovery algorithm.
 *
 * @param advertisedName     Display name used in the BLE advertisement
 * @param restoreIdentifier  Value used for CBPeripheralManagerOptionRestoreIdentifierKey
 * @param queue              The queue this object is called on and callbacks are made on.
 *
 * @return GNSPeripheralManager instance
 */
- (instancetype)initWithAdvertisedName:(NSString *)advertisedName
                     restoreIdentifier:(NSString *)restoreIdentifier
                                 queue:(dispatch_queue_t)queue NS_DESIGNATED_INITIALIZER;

/**
 * Creates a CBPeripheralManager using the main queue for callbacks.
 */
- (instancetype)initWithAdvertisedName:(NSString *)advertisedName
                     restoreIdentifier:(NSString *)restoreIdentifier;

- (instancetype)init NS_UNAVAILABLE;

/**
 * Adds a new peripheral service manager into the managed services. If the GNSPeripheralManager
 * is started, the new peripheral service manager will be added right away into BLE service
 * database. |completion| is called everytime the service is added into the BLE service database.
 *
 * @param peripheralServiceManager GNSPeripheralServiceManager
 * @param completion               Callback called when the service was added.
 */
- (void)addPeripheralServiceManager:(GNSPeripheralServiceManager *)peripheralServiceManager
          bleServiceAddedCompletion:(GNSErrorHandler)completion;

/**
 * If the bluetooth is on, all CB services will be added, and the services will be advertised
 * (according to -[GNSPeripheralServiceManager advertising]. Otherwise, it will be done as soon as
 * the bluetooth is turned on.
 */
- (void)start;

/**
 * Bluetooth advertisement is turned off, and all services are removed from CB.
 */
- (void)stop;

@end
