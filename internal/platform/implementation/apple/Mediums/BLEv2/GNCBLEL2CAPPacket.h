// Copyright 2025 Google LLC
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

typedef NS_ENUM(NSInteger, GNCMBLEL2CAPCommand) {
  // Use to fetch advertisement from server with serviceId.
  GNCMBLEL2CAPCommandRequestAdvertisement = 1,
  // Use to notify the server have fetched all advertisements completely.
  GNCMBLEL2CAPCommandRequestAdvertisementFinish = 2,
  // Use to notify the server current L2CAP socket use for file transferring.
  GNCMBLEL2CAPCommandRequestDataConnection = 3,
  // 11-20 reserved
  // Use to response the advertisement raw data to the client.
  GNCMBLEL2CAPCommandResponseAdvertisement = 21,
  // Use to response the queried service ID not exist in server side.
  GNCMBLEL2CAPCommandResponseServiceIdNotFound = 22,
  // Use to notify the data connection ready
  GNCMBLEL2CAPCommandResponseDataConnectionReady = 23,
  // Use to notify failure for requesting the data connection.
  GNCMBLEL2CAPCommandResponseDataConnectionFailure = 24,
};

NS_ASSUME_NONNULL_BEGIN

/**
 * Represents the format of the BLE L2CAP packet used in L2CAP socket for
 * requesting/responsing.
 *
 * [COMMAND] or [COMMAND][LENGTH][DATA]
 */
@interface GNCBLEL2CAPPacket : NSObject

/// The command of the L2CAP packet.
@property(nonatomic) GNCMBLEL2CAPCommand command;

/// The data of the L2CAP packet.
@property(nonatomic, copy) NSData *_Nullable data;

/**
 * Creates a GNCBLEL2CAPPacket from the received data.
 *
 * @param receivedData The received data from the L2CAP socket.
 * @return The GNCBLEL2CAPPacket instance.
 */
+ (instancetype)receivedPacket:(NSData *)receivedData;

/**
 * Generates a request advertisement packet from the service ID hash.
 *
 * @param serviceIdHash The service ID hash to be used in the packet.
 * @return The generated request advertisement packet.
 */
+ (NSData *)generateRequestAdvertisementPacketFromServiceIdHash:(NSData *)serviceIdHash;

/**
 * Generates a request connection data packet.
 *
 * @return The generated request connection data packet.
 */
+ (NSData *)generateRequestConnectionDataPacket;

/**
 * Generates a response advertisement packet from the advertisement data.
 *
 * @param advertisementData The advertisement data to be used in the packet.
 * @return The generated response advertisement packet.
 */
+ (NSData *)generateResponseAdvertisementPacketFromAdvertisementData:
    (NSData *_Nullable)advertisementData;

/**
 * Generates a response connection data ready packet.
 *
 * @return The generated response connection data ready packet.
 */
+ (NSData *)generateResponseConnectionDataReadyPacket;

/**
 * Returns YES if the L2CAP packet is valid.
 */
- (BOOL)isValid;

/**
 * Returns YES if the L2CAP packet is a fetch advertisement request.
 */
- (BOOL)isFetchAdvertisementRequest;

/**
 * Returns YES if the L2CAP packet is a data connection request.
 */
- (BOOL)isDataConnectionRequest;



@end

NS_ASSUME_NONNULL_END