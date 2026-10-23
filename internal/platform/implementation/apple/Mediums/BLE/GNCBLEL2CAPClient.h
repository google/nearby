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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPStream.h"

@protocol GNCPeripheral;

NS_ASSUME_NONNULL_BEGIN

/// Completion handler for opening an L2CAP stream.
typedef void (^GNCOpenL2CAPStreamCompletionHandler)(GNCBLEL2CAPStream *_Nullable stream,
                                                    NSError *_Nullable error);

/**
 * A block to be invoked after a call to @c disconnect, requesting that the local connection to the
 * remote peripheral be cancelled.
 *
 * @param peripheral The remote peripheral to disconnect from.
 */
typedef void (^GNCRequestDisconnectionHandler)(id<GNCPeripheral> peripheral);

/**
 * An object that can be used to connect BLE over L2CAP on a remote peripheral.
 *
 * @note The public APIs of this class are thread safe.
 */
@interface GNCBLEL2CAPClient : NSObject

- (instancetype)init NS_UNAVAILABLE;

/**
 * Initializes the L2CAP client with a specified request disconnection handler.
 *
 * @param requestDisconnectionHandler Called on a private queue with @c peripheral when the
 *                                    connection to the peripheral should be cancelled.
 */
- (instancetype)initWithRequestDisconnectionHandler:
    (GNCRequestDisconnectionHandler)requestDisconnectionHandler;

/**
 * Opens a L2CAP channel with the @c PSM.
 *
 * @param PSM The PSM to use for opening the L2CAP channel.
 * @param peripheral The peripheral instance.
 * @param completionHandler Called on a private queue with the opened L2CAP stream if successfully
 *                          opened or an error if one has occurred.
 */
- (void)openL2CAPChannelWithPSM:(uint16_t)PSM
                     peripheral:(id<GNCPeripheral>)peripheral
              completionHandler:(GNCOpenL2CAPStreamCompletionHandler)completionHandler;

/** Cancels an active or pending local connection to a peripheral. */
- (void)disconnect;

@end

NS_ASSUME_NONNULL_END
