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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCMConnection.h"

NS_ASSUME_NONNULL_BEGIN

@class GNCBLEL2CAPStream;

/**
 * A GNCBLEL2CAPConnection implementation for BLE L2CAP.
 * This class is thread-safe.
 */
@interface GNCBLEL2CAPConnection : NSObject
@property(nonatomic) GNCMConnectionHandlers *connectionHandlers;

/*
 * Creates a |GNCBLEL2CAPConnection|.
 *
 * @param stream A |GNCL2CAPStream| instance.
 * @param serviceID A string that uniquely identifies the service.
 * @param incomingConnection A flag to indicate the connection is incoming.
 * @param callbackQueue The queue on which all callbacks are made.
 */
+ (instancetype)connectionWithStream:(GNCBLEL2CAPStream *)stream
                           serviceID:(nullable NSString *)serviceID
                 incomingConnection:(BOOL)incomingConnection
                       callbackQueue:(dispatch_queue_t)callbackQueue;

/**
 * Sends data to the remote device.
 *
 * @param payload The data to send.
 * @param completion A block that is called when the data has been sent. The block takes a BOOL
 *        parameter indicating whether the data was sent successfully.
 */
- (void)sendData:(NSData *)payload completion:(void (^)(BOOL))completion;

/**
 * Requests data connection from the remote device.
 *
 * @param completion A block that is called when the data connection is ready. The block takes a
 *        BOOL parameter indicating whether the data connection is ready.
 */
- (void)requestDataConnectionWithCompletion:(void (^)(BOOL))completion;

@end

NS_ASSUME_NONNULL_END
