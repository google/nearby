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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralManager.h"

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket+Private.h"

@class GNSSocket;

typedef BOOL (^GNSUpdateValueHandler)();

/**
 * Private methods called by GNSPeripheralManager, GNSSocket and for tests.
 * Should not be used by the Nearby Socket client.
 */
@interface GNSPeripheralManager ()<CBPeripheralManagerDelegate>

@property(nonatomic, readonly) NSString *restoreIdentifier;
@property(nonatomic, readonly) CBPeripheralManager *cbPeripheralManager;

/**
 * Updates the outgoing characteristic value using an handler. The handler is stored in
 * a queue. If the CBPeripheralManager is ready, the handler is called right away. Otherwise the
 * handler will be called as soon as the CBPeripheralManager is ready.
 * The characteristic value should be updated with
 * -[GNSPeripheralManager updateValue:forCharacteristic:onSocket:]. If the update failed,
 * the handler should return NO, and the handler will be rescheduled when the peripheral will ready
 * again.
 *
 * @param socket  Socket
 * @param handler Handler to update the value. Should return YES if the data was sent successfully,
 *                and NO if the update should be scheduled for later.
 */
- (void)updateOutgoingCharOnSocket:(GNSSocket *)socket withHandler:(GNSUpdateValueHandler)handler;

/**
 * Stops BLE advertising and starts it again with the peripheral service managers who are set to be
 * advertising.
 */
- (void)updateAdvertisedServices;

/**
 * Sends a packet using the outgoing characteristic for a socket.
 *
 * @param data   Packet to send
 * @param socket Socket to send the packet
 *
 * @return YES if the packet has been sent.
 */
- (BOOL)updateOutgoingCharacteristic:(NSData *)data onSocket:(GNSSocket *)socket;

/**
 * Informs the peripheral manager that |socket| is now disconnected.
 *
 * @param socket Socket.
 */
- (void)socketDidDisconnect:(GNSSocket *)socket;

@end
