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
#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * A protocol to wrap nw_listener_t C functions to allow for faking in tests.
 */
@protocol GNCNWListener <NSObject>

/** The port the listener is listening on. */
@property(nonatomic, readonly) uint16_t port;

/**
 * Sets the new connection handler for the listener.
 *
 * @param handler The handler to call when a new connection is received.
 */
- (void)setNewConnectionHandler:(void (^)(nw_connection_t connection))handler;

/**
 * Sets the queue for the listener.
 *
 * @param queue The queue to use for the listener events.
 */
- (void)setQueue:(dispatch_queue_t)queue;

/**
 * Sets the state changed handler for the listener.
 *
 * @param handler The handler to call when the listener state changes.
 */
- (void)setStateChangedHandler:(void (^)(nw_listener_state_t state,
                                         nw_error_t _Nullable error))handler;

/**
 * Starts the listener.
 */
- (void)start;

/**
 * Cancels the listener.
 */
- (void)cancel;

/**
 * Sets the advertise descriptor for the listener.
 *
 * @param advertiseDescriptor The advertise descriptor to set.
 */
- (void)setAdvertiseDescriptor:(nullable nw_advertise_descriptor_t)advertiseDescriptor;

@end

NS_ASSUME_NONNULL_END
