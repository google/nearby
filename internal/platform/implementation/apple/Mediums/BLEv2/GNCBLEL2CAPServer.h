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

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

@class GNCBLEL2CAPStream;
@protocol GNCPeripheralManager;

NS_ASSUME_NONNULL_BEGIN

/**
 * Completion handler for starting listening for an L2CAP channel.
 *
 * @param error The cause of the failure, or @c nil if no error occurred.
 */
typedef void (^GNCStartListeningL2CAPChannelCompletionHandler)(NSError *_Nullable error);

/**
 * An object that publishes the @c PSM value.
 *
 * @note The public APIs of this class are thread safe.
 */
@interface GNCBLEL2CAPServer : NSObject

// Represents a PSM (Protocol/Service Multiplexer) value for an L2CAP channel.
@property(atomic, readonly) CBL2CAPPSM PSM;

/**
 * Creates a L2CAP server with a provided peripheral manager.
 *
 * @param peripheralManager The peripheral manager instance. Note: This is only exposed for testing
 *                          and can be used to inject a fake peripheral manager.
 */
- (instancetype)initWithPeripheralManager:(nullable id<GNCPeripheralManager>)peripheralManager;

/**
 * Starts listening for an L2CAP channel.
 *
 * @param completionHandler The completion handler to call when the L2CAP channel is started.
 */
- (void)startListeningChannelWithCompletionHandler:
    (GNCStartListeningL2CAPChannelCompletionHandler)completionHandler;

/**
 * Accepts an incoming L2CAP connection.
 *
 * @param error The cause of the failure, or @c nil if no error occurred.
 * @return The L2CAP stream, or @c nil if no error occurred.
 */
- (nullable GNCBLEL2CAPStream *)acceptWithError:(NSError **)error;

/**
 * Closes the L2CAP channel
 */
- (void)close;

@end

NS_ASSUME_NONNULL_END
