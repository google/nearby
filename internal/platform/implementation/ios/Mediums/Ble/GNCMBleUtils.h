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

/** Computes a hash from a service ID string. */
NSData *GNCMServiceIDHash(NSString *serviceID);

/** Creates the introduction packet for Ble SocketControlFrame. */
NSData *GNCMGenerateBLEFramesIntroductionPacket(NSData *serviceIDHash);

/**
 * Parses the packet for Ble SocketControlFrame introduction packet and returns
 * serviceIdHash if succeed.
 */
NSData *GNCMParseBLEFramesIntroductionPacket(NSData *data);

/** Creates the disconnection packet for Ble SocketControlFrame. */
NSData *GNCMGenerateBLEFramesDisconnectionPacket(NSData *serviceIDHash);

/**
 * Calls the completion handler with (a) YES if the GNSSocket connected, or (b) NO if it failed to
 * connect for any reason. The completion handler is called on the main queue.
 */
void GNCMWaitForConnection(GNSSocket *socket, GNCMBoolHandler completion);

#ifdef __cplusplus
}  // extern "C"
#endif

NS_ASSUME_NONNULL_END
