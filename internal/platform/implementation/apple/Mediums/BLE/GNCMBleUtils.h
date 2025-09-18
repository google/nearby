// Copyright 2022 Google LLC
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

@class GNSSocket;

typedef void (^GNCMBoolHandler)(BOOL flag);

#ifdef __cplusplus
extern "C" {
#endif

/** Required lengths of certain BLE advertisement fields. */
typedef NS_ENUM(NSUInteger, GNCMBleAdvertisementLength) {
  /** Length of service ID hash data object, used in the BLE advertisement and the packet prefix. */
  GNCMBleAdvertisementLengthServiceIDHash = 3,
};

typedef NS_ENUM(NSUInteger, GNCMBLEL2CAPCommand) {
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

/**
 * Represents the format of the BLE L2CAP packet used in L2CAP socket for
 * requesting/responsing.
 *
 * [COMMAND] or [COMMAND][LENGTH][DATA]
 */
@interface GNCMBLEL2CAPPacket : NSObject
@property(nonatomic, readonly) GNCMBLEL2CAPCommand command;
@property(nonatomic, readonly, nullable) NSData *data;

- (instancetype)initWithCommand:(GNCMBLEL2CAPCommand)command
                           data:(nullable NSData *)data NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
@end

/** Computes a hash from a service ID string. */
NSData *GNCMServiceIDHash(NSString *serviceID);

/** Creates the introduction packet for Ble SocketControlFrame. */
NSData *GNCMGenerateBLEFramesIntroductionPacket(NSData *serviceIDHash);

/**
 * Parses the packet for Ble SocketControlFrame introduction packet and returns
 * serviceIdHash if succeed.
 */
NSData *_Nullable GNCMParseBLEFramesIntroductionPacket(NSData *_Nullable data);

/** Creates the disconnection packet for Ble SocketControlFrame. */
NSData *GNCMGenerateBLEFramesDisconnectionPacket(NSData *serviceIDHash);

/** Creates the packet acknowledgement packet for Ble SocketControlFrame. */
NSData *GNCMGenerateBLEFramesPacketAcknowledgementPacket(NSData *serviceIDHash, int receivedSize);

/** Parses the BLE L2CAP packet from the data. */
GNCMBLEL2CAPPacket *_Nullable GNCMParseBLEL2CAPPacket(NSData *_Nullable data);

/** Creates the BLE L2CAP packet from the command and data. */
NSData *_Nullable GNCMGenerateBLEL2CAPPacket(GNCMBLEL2CAPCommand command, NSData *_Nullable data);

/**
 * Calls the completion handler with (a) YES if the GNSSocket connected, or (b) NO if it failed to
 * connect for any reason. The completion handler is called on the main queue.
 */
void GNCMWaitForConnection(GNSSocket *socket, GNCMBoolHandler completion);

#ifdef __cplusplus
}  // extern "C"
#endif

NS_ASSUME_NONNULL_END
