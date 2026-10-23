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

#import <Foundation/Foundation.h>

@class CBUUID;
@class GNSCentralPeerManager;
@class GNSSocket;

typedef void (^GNSCentralSocketCompletion)(GNSSocket *socket, NSError *error);
typedef void (^GNSPairingCompletion)(BOOL pairing, NSError *error);
typedef void (^GNSReadRRSIValueCompletion)(NSNumber *rssi, NSError *error);

/**
 * This class manage one CBPeripheral. It can only manage one service. The service UUID is set
 * by GNSCentralManager. The GNSCentralPeerManager instance is created by GNSCentralManager.
 * From this class a socket can be created using -[GNSCentralPeerManager socketWithCompletion:].
 * See GNSCentralManager header for more information.
 *
 * This class is not thread-safe.
 */
@interface GNSCentralPeerManager : NSObject

/**
 * MTU value. The default is 100.
 */
@property(nonatomic) NSUInteger socketMaximumUpdateValueLength;

/**
 * Peer identifier.
 */
@property(nonatomic, readonly) NSUUID *identifier;

- (instancetype)init NS_UNAVAILABLE;

/**
 * Creates the socket. When the socket is created and ready to use, |completion| is called. |data|
 * is sent during the connection handshake, and is going to be treated as the first message on the
 * peripheral/server, as established by the Weave BLE protocol. |data| can contain at most 13 bytes.
 *
 * If the socket fails to be created |completion| is called with an error. This method should be
 * called once until the completion is called, otherwise |completion| will be called with an
 * operation in progress error.
 *
 * @param handshakeData Data sent during the connection handshake. Should contain at most 13 bytes.
 * @param pairingCharacteristic If YES, peripheral need to have the pairing characteristic in order
 *     to get the socket. The pairing characteristic is required to trigger the pairing.
 * @param completion Handler called when the socket is created or failed to be created. It is
 *     guaranteed that either the socket or the error passed to |completion| are not nil.
 */
- (void)socketWithHandshakeData:(NSData *)handshakeData
          pairingCharacteristic:(BOOL)hasPairingCharacteristic
                     completion:(GNSCentralSocketCompletion)completion;

/**
 * See -[GNSCentralPeerManager socketWithHandshakeData:pairingCharacteristic:completion:].
 */
- (void)socketWithPairingCharacteristic:(BOOL)shouldAddPairingCharacteristics
                             completion:(GNSCentralSocketCompletion)completion;

/**
 * Cancel the pending socket request, if any. |completion| (passed in
 * |socketWithPairingCharacteristic:completion:|) will be called (with a nil socket) when the
 * pending socket request is successfully cancelled (and disconnected).
 *
 * Note that, after calling |cancelPendingSocket| is still necessary to wait for the |completion|
 * (passed in a previous |socketWithPairingCharacteristic:completion:| invocation) to be called
 * before calling |socketWithPairingCharacteristic:completion:| again.
 */
- (void)cancelPendingSocket;

/**
 * Triggers pairing between the central and the peripheral. The socket must have been generated
 * with pairing characteristic. The completion has to be called before calling this method again.
 *
 * @param completion Completion called when the pairing has failed or succeed.
 */
- (void)startBluetoothPairingWithCompletion:(GNSPairingCompletion)completion;

/**
 * Retrieve the RSSI value while the central peer is connected. If a completion is pending while
 * this method is called again, the second completion will be added.
 *
 * @param completion Called when the value is available. Cannot be nil.
 */
- (void)readRSSIWithCompletion:(GNSReadRRSIValueCompletion)completion;

@end
