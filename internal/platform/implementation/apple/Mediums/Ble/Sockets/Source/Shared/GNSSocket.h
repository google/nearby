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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSUtils.h"

@class CBCentral;
@class GNSPeripheralServiceManager;
@class GNSSocket;

// |progress| contains the percentage of the message already sent.
typedef void (^GNSProgressHandler)(float progress);

/**
 * Protocol for socket delegates. The socket delegate should be set when the socket is accepted
 * (when used with GNSPeripheralServiceManager) or when the socket is created (when used with
 * GNSCentralPeer).
 */
@protocol GNSSocketDelegate<NSObject>

/**
 * Called when the socket is ready to send or receive data.
 *
 * @param socket Socket
 */
- (void)socketDidConnect:(GNSSocket *)socket;

/**
 * Called when the socket has been disconnected by the central (because a disconnect signal was
 * received or -[GNSSocket disconnect] has been called).
 *
 * @param socket Socket
 * @param error  Error that caused the socket to disconnect. Nil if the socket was disconnected
 *     properly (by calling -[GNSSocket disconnect] on this side or the other side of the socket).
 */
- (void)socket:(GNSSocket *)socket didDisconnectWithError:(NSError *)error;

/**
 * Called when a new message has been received.
 *
 * @param socket Socket
 * @param data   Message received
 */
- (void)socket:(GNSSocket *)socket didReceiveData:(NSData *)data;

@end

/**
 * This class is in charge of receiving and sending data between one central and one peripheral. It
 * is created and owned by GNSPeripheralServiceManager or GNSCentralPeer.
 *
 *
 *  * To create a socket, see the GNSPeripheralManager or GNSCentralPeer documentation.
 * When the socket is created, set the socket delegate. As soon as it is ready to use, the delegate
 * is called:
 * - (void)socketDidConnect:(GNSSocket *)socket {
 *   // the socket is ready to be used. To send or receive data.
 * }
 *
 *  * To receive data:
 * - (void)socket:(GNSSocket *)socket didReceiveData:(NSData *)data {
 *   NSLog(@"Data received from central %@", data);
 *   ...
 * }
 *
 *  * To send data:
 * GNSErrorHandler completionHandler = ^(NSError *error) {
 *   if (error) {
 *     NSLog(@"Failed to send data")
 *   } else {
 *     NSLog(@"data has been sent");
 *   }
 * }
 * [socket sendData:dataToSend
 *          completion:completionHandler];
 *
 *  * To disconnect:
 * [mySocket disconnect];
 *
 *  * Once the socket is disconnected (by -[GNSSocket disconnect] or by the peer):
 * - (void)socket:(GNSSocket *)socket didDisconnectWithError:(NSError *)error {
 *   NSLog(@"Socket disconnected, by peer");
 *   ...
 * }
*/
@interface GNSSocket : NSObject

/**
 * The socket delegate.
 */
@property(nonatomic, weak) id<GNSSocketDelegate> delegate;

/**
 * YES if the socket is connected. The physical BLE connection may still exist when this is NO.
 */
@property(nonatomic, readonly, getter=isConnected) BOOL connected;

/**
 * The Core Bluetooth peer identifier.
 */
@property(nonatomic, readonly) NSUUID *peerIdentifier;

/**
 * The Core Bluetooth service identifier.
 */
@property(nonatomic, readonly) NSUUID *serviceIdentifier;

/**
 * The socket identifier.
 */
@property(nonatomic, readonly) NSUUID *socketIdentifier;

/**
 * A peer socket is always created by the library.
 */
- (instancetype)init NS_UNAVAILABLE;

/**
 * Returns YES if there is already a send operation in progress. If this method returns NO,
 * then a call to -[GNSSocket sendData:completion:] won't result in a |GNSErrorOperationInProgress|
 * error.
 */
- (BOOL)isSendOperationInProgress;

/**
 * Sends data to the central. The completion has to be called before calling this method a second
 * time.
 *
 * @param data            Data to send
 * @param progressHandler Called repeatedly for progress feedback while the data is being sent.
 * @param completion      Callback called when the data has been fully sent,
 *                        or it has failed to be sent (socket not connected or disconnected).
 */
- (void)sendData:(NSData *)data
    progressHandler:(GNSProgressHandler)progressHandler
         completion:(GNSErrorHandler)completion;

/**
 * Sends the disconnect signal and then disconnects this connection. The socket cannot be
 * disconnected if the socket is processing data to send. The current send data operation has
 * to be cancelled first.
 */
- (void)disconnect;

@end
