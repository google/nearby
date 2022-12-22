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

#import "internal/platform/implementation/apple/Mediums/GNCMConnection.h"

NS_ASSUME_NONNULL_BEGIN

@class GNSSocket;

/**
 * A GNCMConnection implementation for BLE.
 * This class is thread-safe.
 */
@interface GNCMBleConnection : NSObject <GNCMConnection>
@property(nonatomic) GNCMConnectionHandlers *connectionHandlers;

/**
 * Creates a |GNCMBleConnectiom|.
 *
 * @param socket A |GNSSocket| instance.
 * @param serviceID A string that uniquely identifies the service.
 * @param expectedIntroPacket A flag to indicate the connection is expecting the
 *                            introduction packet.
 * @param callbackQueue The queue on which all callbacks are made.
 */
+ (instancetype)connectionWithSocket:(GNSSocket *)socket
                           serviceID:(nullable NSString *)serviceID
                 expectedIntroPacket:(BOOL)expectedIntroPacket
                       callbackQueue:(dispatch_queue_t)callbackQueue;
@end

NS_ASSUME_NONNULL_END
