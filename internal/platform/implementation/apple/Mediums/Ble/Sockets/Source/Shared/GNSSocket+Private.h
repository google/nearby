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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket.h"

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSUtils+Private.h"

@class GNSWeaveDataPacket;

/**
 * Protocol implemented by the class who is in charge to create the socket. The owner knows how
 * send and receive data through Bluetooth BLE.
 */
@protocol GNSSocketOwner<NSObject>

/**
 * Returns the current MTU. Called by the socket when it needs to create chunks to send data.
 *
 * @param socket Current socket
 *
 * @return MTU.
 */
- (NSUInteger)socketMaximumUpdateValueLength:(GNSSocket *)socket;

/**
 * Called by the socket when it needs to send a chunk of data.
 *
 * @param data        Chunk to send
 * @param completion  Callback called when the data has been fully sent or an error has occurred.
 */
- (void)sendData:(NSData *)data socket:(GNSSocket *)socket completion:(GNSErrorHandler)completion;

/**
 * Returns the UUID of the peer.
 *
 * @param socket Current socket
 *
 * @return UUID based on the peer.
 */
- (NSUUID *)socketServiceIdentifier:(GNSSocket *)socket;

/**
 * Called by the socket when the socket should be disconnected.
 *
 * @param socket     Current socket
 */
- (void)disconnectSocket:(GNSSocket *)socket;

/**
 * Called when the socket is deallocated. Some owners (like GNSCentralPeerManager) doesn't retain
 * the socket and let the socket user taking care of the retain/release. When this method is called
 * the socket should not be called anymore.
 *
 * @param socket Socket being deallocated.
 */
- (void)socketWillBeDeallocated:(GNSSocket *)socket;

@end

/**
 * Private methods called by GNSSocket and for tests.
 * Should not be used by the Nearby Socket client.
 */
@interface GNSSocket ()

@property(nonatomic, readwrite) UInt16 packetSize;
@property(nonatomic, readonly) UInt8 receivePacketCounter;
@property(nonatomic, readonly) UInt8 sendPacketCounter;
@property(nonatomic, readonly) id<GNSSocketOwner> owner;
@property(nonatomic, readonly) dispatch_queue_t queue;

- (instancetype)initWithOwner:(id<GNSSocketOwner>)owner
                  centralPeer:(CBCentral *)centralPeer
                  queue:(dispatch_queue_t)queue;
- (instancetype)initWithOwner:(id<GNSSocketOwner>)owner
               peripheralPeer:(CBPeripheral *)peripheralPeer
                        queue:(dispatch_queue_t)queue;

/**
 * Increments |receivePacketCounter|.
 */
- (void)incrementReceivePacketCounter;

/**
 * Increments |sendPacketCounter|.
 */
- (void)incrementSendPacketCounter;

/**
 * Called by the socket owner once the connection response packet has been sent or received.
 * The socket is ready to be used when this method is called.
 */
- (void)didConnect;

/**
 * Called by the socket owner when the socket is disconnected and should not be used anymore.
 * The socket delegate is called to be notified with:
 * -[id<GNSSocketDelegate> socket:didDisconnectWithError:]
 *
 * @param error Error that caused the socket to disconnect. Nil if the socket was disconnected
 *     properly.
 */
- (void)didDisconnectWithError:(NSError *)error;

/**
 * Called by the socket owner when a data packet is received.
 *
 * @param dataPacket The data packet received.
 */
- (void)didReceiveIncomingWeaveDataPacket:(GNSWeaveDataPacket *)dataPacket;

/**
 * Returns YES if an incoming message is pending and more chunks are waited.
 *
 * @return BOOL
 */
- (BOOL)waitingForIncomingData;

/**
 * Returns the peer casted as CBPeripheral. Asserts if the socket was created with another type.
 *
 * @return Peer.
 */
- (CBPeripheral *)peerAsPeripheral;

/**
 * Returns the peer casted as CBCentral. Asserts if the socket was created with another type.
 *
 * @return Peer.
 */
- (CBCentral *)peerAsCentral;

@end
