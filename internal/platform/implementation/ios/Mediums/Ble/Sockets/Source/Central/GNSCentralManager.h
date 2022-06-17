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

NS_ASSUME_NONNULL_BEGIN

@class CBUUID;
@class GNSCentralManager;
@class GNSCentralPeerManager;

@protocol GNSCentralManagerDelegate<NSObject>

/**
 * Called while scanning when a new peer is found.
 *
 * @param centralManager     Central manager
 * @param centralPeerManager New central peer.
 * @param advertisementData  The advertisement data received from the peer.
 */
- (void)centralManager:(GNSCentralManager *)centralManager
       didDiscoverPeer:(GNSCentralPeerManager *)centralPeerManager
     advertisementData:(nullable NSDictionary *)advertisementData;

/**
 * Called when the Bluetooth Low Energy state is updated. Note that scanning is paused if Bluetooth
 * is disabled while scanning is on. Scanning will be resumed automatically as soon as Bluetooth is
 * re-enabled.
 *
 * @param centralManager Central manager
 */
- (void)centralManagerDidUpdateBleState:(GNSCentralManager *)centralManager;

@end

/**
 * GNSCentralManager can search for new GNSCentralPeerManager or can retrieve known
 * GNSCentralPeerManager. Note that GNSCentralPeerManager retains its GNSCentralManager. Therefore
 * it is not necessary to keep a strong pointer to GNSCentralManager.
 *
 *  * To create a GNSCentralManager instance:
 * GNSCentralManager *centralManager = [[GNSCentralManager alloc] initWithSocketServiceUUID:UUID];
 * centralManager.delegate = myManagerDelegate;
 *
 *  * Get a peer:
 *    - To retrieve a known peer:
 *      With a known peer: -[GNSCentralManager retrieveCentralPeerWithIdentifier:]
 *      To retrieve a previous peer,  can be called instead of scanning.
 *
 *    - To scan for peers:
 *      -[GNSCentralManager startScanWithAdvertisedName:advertisedServiceUUID:]
 *      When a peer has been discovered, myManagerDelegate is called:
 *      -[myManagerDelegate centralManager:discoveredPeer:];
 *
 *  * To create a socket to the peer:
 * With the received peer, -[GNSCentralPeerManager socketWithPairingCharacteristic:completion:]
 * has to be called:
 * [centralPeer socketWithPairingCharacteristic:shouldAddPairingCharacteristics
 *                                   completion:^(GNSSocket *mySocket, NSError *error) {
 *                                    if (error) {
 *                                      NSLog(@"Error to get the socket %@", error);
 *                                      return;
 *                                    }
 *                                    mySocket.delegate = mySocketDelegate;
 *                                  }];
 *
 * To use the socket, please see GNSSocket.
 *
 * Once the socket is disconnected, the peer will automatically be disconnected from BLE. Also, if
 * GNSCentralPeerManager instance is deallocated, the socket will be automatically closed.
 *
 * This class is not thread-safe.
 */
@interface GNSCentralManager : NSObject

/**
 * Service used for the socket.
 */
@property(nonatomic, readonly) CBUUID *socketServiceUUID;

/**
 * YES, if scanning is enabled (even when the bluetooth is disabled).
 */
@property(nonatomic, readonly, getter=isScanning) BOOL scanning;
@property(nonatomic, readwrite, weak) id<GNSCentralManagerDelegate> delegate;

/**
 * BLE state based on -[CBCentralManager state].
 */
@property(nonatomic, readonly) CBCentralManagerState cbCentralManagerState;

- (instancetype)init NS_UNAVAILABLE;

/**
 * Creates an instance of GNSCentralManager. Only one service is supported. This service will be
 * passed to the GNSCentralPeerManager instances and GNSSocket.
 *
 * @param socketServiceUUID UUID used to create peer sockets with all the peers found.
 * @param queue             The queue this object is called on and callbacks are made on.
 *
 * @return GNSCentralManager instance
 */
- (nullable instancetype)initWithSocketServiceUUID:(CBUUID *)socketServiceUUID
                                             queue:(dispatch_queue_t)queue
    NS_DESIGNATED_INITIALIZER;

/**
 * Creates an instance of GNSCentralManager using the main queue for callbacks.
 */
- (nullable instancetype)initWithSocketServiceUUID:(CBUUID *)socketServiceUUID;

/**
 * TODO(sacomoto): Cleanup and merge this and the method below.
 *
 * Starts scanning for all Bluetooth Low Energy peers that advertise the service
 * |advertisedServiceUUID| or the name |advertisedName|.
 *
 * If |advertisedName| is non-nil, this central manager will scan for all peripherals and filter
 * peripherals that advertise |advertisedServiceUUID| or |advertisedName|.
 *
 * If |advertisedServiceUUID| is non-nil, the central manager will scan for all peripherals that
 * advertise |self.socketServiceUUID|.
 *
 * If both |advertisedName| and |advertisedServiceUUID| are nil, the central manager will scan for
 * all peripherals without any filtering.
 *
 * Note that |self.socketServiceUUID| and |advertisedServiceUUID| may be different. In this case,
 * the manager will scan for devices advertising |advertisedServiceUUID| but will use
 * |self.socketServiceUUID| to transfer data.
 *
 * The delegate will be notified when each peer is found.
 *
 * @param advertisedName        The name advertised by peripheral.
 * @param advertisedServiceUUID The service advertised by the peripheral.
 */
- (void)startScanWithAdvertisedName:(nullable NSString *)advertisedName
              advertisedServiceUUID:(nullable CBUUID *)advertisedServiceUUID;

/**
 * Starts scanning for all Bluetooth Low Energy peers that advertise some service in
 * |advertisedServiceUUIDs|. The delegate will be notified when each peer is found.
 *
 * @param advertisedServiceUUID The service advertised by the peripheral.
 */
- (void)startScanWithAdvertisedServiceUUIDs:(nullable NSArray<CBUUID *> *)advertisedServiceUUIDs;

/**
 * Stops scanning.
 */
- (void)stopScan;

/**
 * Starts "no-scan" mode, where identifiers of peripherals already discovered can be be passed into
 * |-retrievePeripheralWithIdentifier:advertisementData:| for use as socket peripherals. If valid,
 * the delegate will be notified.  The peripheral must be advertising a service in
 * |advertisedServiceUUIDs|.
 *
 * @param advertisedServiceUUID The service(s) the peripherals must be advertising.
 */
- (void)startNoScanModeWithAdvertisedServiceUUIDs:
    (nullable NSArray<CBUUID *> *)advertisedServiceUUIDs;

/**
 * Tries to retrieve a peripheral that matches the given identifier, and, if successful, treats it
 * as a newly found peripheral.
 *
 * @param identifier The peripheral identifier.
 * @param advertisementData The advertisement data associated with the peripheral.
 */
- (void)retrievePeripheralWithIdentifier:(NSUUID *)identifier
                       advertisementData:(NSDictionary<NSString *, id> *)advertisementData;

/**
 * Stops "no-scan" mode.
 */
- (void)stopNoScanMode;

/**
 * Creates a GNSCentralPeerManager for an identifier.
 *
 * @param identifier Identifier from the idenfier.
 *
 * @return Central peer.
 */
- (nullable GNSCentralPeerManager *)retrieveCentralPeerWithIdentifier:(NSUUID *)identifier;

@end

NS_ASSUME_NONNULL_END
