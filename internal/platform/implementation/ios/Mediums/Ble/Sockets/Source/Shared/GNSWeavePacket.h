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

NS_ASSUME_NONNULL_BEGIN

@class GNSWeaveConnectionRequestPacket;
@class GNSWeaveConnectionConfirmPacket;
@class GNSWeaveErrorPacket;
@class GNSWeaveDataPacket;

extern const UInt8 kGNSMaxPacketCounterValue;
extern const UInt16 kGNSMinSupportedPacketSize;
extern const NSUInteger kGNSMaxCentralHandshakeDataSize;

typedef NS_ENUM(UInt8, GNSWeaveControlCommand) {
  GNSWeaveControlCommandConnectionRequest = 0,
  GNSWeaveControlCommandConnectionConfirm = 1,
  GNSWeaveControlCommandError = 2,
};

/**
 * This protocol should be implemented by classes handling Weave BLE packets. The classes should
 * implement only the methods corresponding to the packets it should handle. It should be used with
 * +[GNSWeavePacket parsePacket:] and -[GNSWeavePacket visitWithHandler:context:] to parse
 * serialized Weave packets.
 *
 * For example, the |Socket| class below handles a serialized weave connection request packet:
 *
 * @interface Socket : NSObject<GNSWeavePacketHandler>
 * @end
 *
 * @implementation Socket
 *
 * - (void)didReceivedData:(NSData *)data {
 *   GNSWeavePacket *packet = [GNSWeavePacket parseData:data error:nil];
 *   if ([packet visitWithHandler:self context:nil]) {
 *     NSLog(@"This class can handle this packet.");
 *   } else {
 *     NSLog(@"Unexpected packet received.");
 *   }
 * }
 *
 * - (void)handleConnectionRequestPacket:(GNSWeaveConnectionRequestPacket *)packet
 *                               context:(nullable id)context {
 *   NSLog(@"Connection request packet received.");
 *   ...
 * }
 * @end
 *
 **/
@protocol GNSWeavePacketHandler<NSObject>
@optional

- (void)handleConnectionRequestPacket:(GNSWeaveConnectionRequestPacket *)packet
                              context:(nullable id)context;
- (void)handleConnectionConfirmPacket:(GNSWeaveConnectionConfirmPacket *)packet
                              context:(nullable id)context;
- (void)handleErrorPacket:(GNSWeaveErrorPacket *)packet context:(nullable id)context;
- (void)handleDataPacket:(GNSWeaveDataPacket *)packet context:(nullable id)context;

@end

/**
 * The Weave BLE protocol (go/weave-ble-gatt-transport) has two types of packets: control and data.
 *
 * There are 3 types of control packets:
 *      - connection request (GNSWeaveConnectionRequestPacket);
 *      - connection confirm (GNSWeaveConnectionConfirmPacket);
 *      - error (GNSWeaveErrorPacket).
 *
 * The first two messages are used to establish the Weave BLE logical connection: the central
 * (client) sends a connection request packet and the peripheral replies with a connection confirm
 * request. This first two messages are used to negociate to connection paramenter: protocol version
 * and packet size.
 *
 * After the logical connection is established the peers can exchange arbitrarily large messages
 * that split are into data packets (GNSWeaveDataPacket).
 *
 * All packets have a 3-bit packet counter. This is used to detect packet drops, re-ordering or
 * duplication. There is no recovery strategy, if a peer detects any error it sends an error packet
 * to the other peer and closes the connection.
 **/
@interface GNSWeavePacket : NSObject
@property(nonatomic, readonly) UInt8 packetCounter;

/**
 * Parses |data| and, if possible, extracts and return the corresponding Weave packet. It returns
 * nil if there was an error parsing the packet. See GNSWeavePacketHandler.
 *
 * @param data The binary data containing.
 * @param outError The error causing the parsing to fail.
 *
 * @return The corresponding Weave packet or an nil if there was an error.
 **/
+ (nullable GNSWeavePacket *)parseData:(NSData *)data
                                 error:(out __autoreleasing NSError **)outError;

- (instancetype)init NS_UNAVAILABLE;

/**
 * Calls the |handler| method corresponding to the type of the current packet. See
 * GNSWeavePacketHandler.
 *
 * @param handler The packet handler.
 * @param context The context passed to the handler.
 *
 * @return YES if |handler| can handle the current message.
 */
- (BOOL)visitWithHandler:(id<GNSWeavePacketHandler>)handler context:(nullable id)context;

/**
 * Serialize the packet.
 */
- (NSData *)serialize;

@end

@interface GNSWeaveConnectionRequestPacket : GNSWeavePacket

@property(nonatomic, readonly) UInt16 minVersion;
@property(nonatomic, readonly) UInt16 maxVersion;
@property(nonatomic, readonly) UInt16 maxPacketSize;
@property(nonatomic, readonly) NSData *data;

/**
 * Creates an instance of the GNSWeaveConnectionRequestPacket. Note: there is no packet counter
 * parameter as this is necessarily the first packet sent by this peer (central/client).
 *
 * @param minVersion The minimum Weave BLE protocol version supported by this peer (central/client).
 * @param maxVersion The maximum Weave BLE protocol version supported by this peer.
 * @param maxPacketSize The maximum packet size (in bytes) supported by this peer. According to the
 * BLE specs this should be at least 20 and at most 509 bytes.
 * @param data The optional data send with the connection request packet. This should not exceed
 * 13 bytes
 *
 * @return GNSWeaveConnectionRequestPacket instance.
 **/
- (nullable instancetype)initWithMinVersion:(UInt16)minVersion
                                 maxVersion:(UInt16)maxVersion
                              maxPacketSize:(UInt16)maxPacketSize
                                       data:(nullable NSData *)data NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithPacketCounter:(UInt8)packetCounter NS_UNAVAILABLE;

@end

@interface GNSWeaveConnectionConfirmPacket : GNSWeavePacket

@property(nonatomic, readonly) UInt16 version;
@property(nonatomic, readonly) UInt16 packetSize;
@property(nonatomic, readonly) NSData *data;

/**
 * Creates an instance of the GNSWeaveConnectionConfirmPacket. Note: there is no packet counter
 * parameter as this is necessarily the first packet sent by this peer (peripheral/server).
 *
 * @param version The chosen Weave BLE protocol version for the current connection.
 * @param packetSize The chosen packet size for the current connection. According to the BLE specs
 * this should be at least 20 and at most 509 bytes.
 * @param data The optional data send with the connection confirm packet. This should not exceed 15
 * bytes.
 *
 * @return GNSWeaveConnectionConfirmPacket instance.
 **/
- (nullable instancetype)initWithVersion:(UInt16)version
                              packetSize:(UInt16)packetSize
                                    data:(nullable NSData *)data NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithPacketCounter:(UInt8)packetCounter NS_UNAVAILABLE;

@end

@interface GNSWeaveErrorPacket : GNSWeavePacket

/**
 * Creates an instance of the GNSWeaveErrorPacket.
 *
 * @param packetCounter The current 3-bit packet counter (i.e. should be stricly smaller than 8).
 *
 * @return GNSWeaveErrorPacket instance.
 **/
- (nullable instancetype)initWithPacketCounter:(UInt8)packetCounter NS_DESIGNATED_INITIALIZER;

@end

@interface GNSWeaveDataPacket : GNSWeavePacket
@property(nonatomic, readonly, getter=isFirstPacket) BOOL firstPacket;
@property(nonatomic, readonly, getter=isLastPacket) BOOL lastPacket;
@property(nonatomic, readonly) NSData *data;

/**
 * Creates an instance of the GNSWeaveDataPacket for |data| starting at |outOffset| containing at
 * most |packetSize|-1 bytes, and updates |outOffset| for the next packet. This should be used
 * iteratively to split a message in GNSWeaveDataPacket's to be send to other peer.
 *
 * @param packetCounter The current 3-bit packet counter (i.e. should be stricly smaller than 8).
 * @param packetSize The maximum size of a data packet (including the 1-byte header).
 * @param data The data to send to the other peer.
 * @param inOutOffset The offset for |data|. It's updated with the new offset value for the next
 * data packet. It's equal to |data.length| if this is the last packet.
 *
 * @return GNSWeaveDataPacket instance.
 **/
+ (nullable GNSWeaveDataPacket *)dataPacketWithPacketCounter:(UInt8)packetCounter
                                                  packetSize:(UInt16)packetSize
                                                        data:(NSData *)data
                                                      offset:(NSUInteger *)inOutOffset;

/**
 * Creates an instance of the GNSWeaveDataPacket. Avoid using this initializer directly, prefer
 * using -[GNSWeaveDataPacket dataPacketWithPacketCounter:packetSize:data:offset:].
 *
 * @param packetCounter The current 3-bit packet counter (i.e. should be stricly smaller than 8).
 * @param isFirstPacket YES if this is the first packet of a message.
 * @param isLastPacket YES if this is the last packet of a message.
 * @param data The actual data being send.
 *
 * @return GNSWeaveDataPacket instance.
 **/
- (nullable instancetype)initWithPacketCounter:(UInt8)packetCounter
                                   firstPacket:(BOOL)isFirstPacket
                                    lastPacket:(BOOL)isLastPacket
                                          data:(NSData *)data NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithPacketCounter:(UInt8)packetCounter NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
