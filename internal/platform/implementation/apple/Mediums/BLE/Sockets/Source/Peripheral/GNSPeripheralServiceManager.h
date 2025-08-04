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

@class GNSPeripheralManager;
@class GNSPeripheralServiceManager;
@class GNSSocket;

typedef BOOL (^GNSShouldAcceptSocketHandler)(GNSSocket *socket);

/**
 * This class manages one BLE service. It keeps track of the list of centrals connected with
 * a socket to this service. This class has also the knowledge to parse the |GNSControlSignal|.
 * One instance is created per BLE service and each instance can be used only once.
 *
 * This class is not thread-safe.
 */
@interface GNSPeripheralServiceManager : NSObject

/**
 * Core Bluetooth service UUID.
 */
@property(nonatomic, readonly) CBUUID *serviceUUID;

/**
 * Makes this service being advertised by Core Bluetooth or not. Even if the service is not
 * advertised, the service is still present in the service database, and thus can still be used
 * to send/receive data. The default value is YES.
 */
@property(nonatomic, getter=isAdvertising) BOOL advertising;

/**
 * Creates an instance to manage a BLE service. The BLE service can be used to create one socket
 * per central. When a central tries to connected, |shouldAcceptSocketHandler| is called with the
 * socket to the central. If the handler returns YES, the socket is accepted. The socket delegate
 * should be set before returning from |shouldAcceptSocketHandler| (if the socket is accepted).
 * The socket will be ready to use shortly after.
 * -[id<GNSSocketDelegate> socketDidConnect:] will be called once the socket
 * is ready to send and receive data. The socket is retained by GNSPeripheralServiceManager as
 * long as it is connected.
 *
 * @param serviceUUID               Service UUID
 * @param addPairingCharacteristic  If YES, will advertise an encrypted characteristic used by
 *     the central if it wants to do a bluetooth pairing
 * @param shouldAcceptSocketHandler Handler called when a central starts a new socket. The delegate
 *     socket can be set if the socket is accepted.
 * @param queue                     The queue this object is called on and callbacks are made on.
 *
 * @return GNSPeripheralServiceManager instance
 */
- (instancetype)initWithBleServiceUUID:(CBUUID *)serviceUUID
              addPairingCharacteristic:(BOOL)addPairingCharacteristic
             shouldAcceptSocketHandler:(GNSShouldAcceptSocketHandler)shouldAcceptSocketHandler
                                 queue:(dispatch_queue_t)queue
    NS_DESIGNATED_INITIALIZER;

/**
 * Creates an instance using the main queue for callbacks.
 *
 * @return GNSPeripheralServiceManager instance
 */
- (instancetype)initWithBleServiceUUID:(CBUUID *)serviceUUID
              addPairingCharacteristic:(BOOL)addPairingCharacteristic
             shouldAcceptSocketHandler:(GNSShouldAcceptSocketHandler)shouldAcceptSocketHandler;

- (instancetype)init NS_UNAVAILABLE;

@end
