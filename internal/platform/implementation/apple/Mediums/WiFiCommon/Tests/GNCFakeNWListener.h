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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWListener.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * Fake implementation of GNCNWListener for testing purposes.
 */
@interface GNCFakeNWListener : NSObject <GNCNWListener>

#pragma mark - Properties for Controlling Behavior

/**
 * If set, `start` will asynchronously call the state changed handler with a `failed` state.
 */
@property(nonatomic, nullable) NSError *simulatedError;

/** The port number to return from the `port` property. */
@property(nonatomic) uint16_t port;

/**
 * An optional block to be executed when `setStateChangedHandler:` is called. Useful for
 * tests that need to know when the handler has been set.
 */
@property(nonatomic, nullable) dispatch_block_t stateHandlerSetBlock;

#pragma mark - Properties for Verifying Interactions

/** The handler block captured from the last call to `setNewConnectionHandler:`. */
@property(nonatomic, nullable) void (^capturedNewConnectionHandler)(nw_connection_t connection);

/** The queue captured from the last call to `setQueue:`. */
@property(nonatomic, nullable) dispatch_queue_t capturedQueue;

/** The handler block captured from the last call to `setStateChangedHandler:`. */
@property(nonatomic, nullable) void (^capturedStateChangedHandler)
    (nw_listener_state_t state, nw_error_t _Nullable error);

/** Whether `start` has been called. */
@property(nonatomic) BOOL startCalled;

/** Whether `cancel` has been called. */
@property(nonatomic) BOOL cancelCalled;

/** Whether `setAdvertiseDescriptor:` has been called. */
@property(nonatomic) BOOL setAdvertiseDescriptorCalled;

/** The descriptor captured from the last call to `setAdvertiseDescriptor:`. */
@property(nonatomic, nullable) nw_advertise_descriptor_t capturedAdvertiseDescriptor;

@end

NS_ASSUME_NONNULL_END
