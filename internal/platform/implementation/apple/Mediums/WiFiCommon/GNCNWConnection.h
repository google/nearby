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
 * A protocol to wrap nw_connection_t C functions to allow for faking in tests.
 */
@protocol GNCNWConnection <NSObject>

/**
 * Creates a connection to an endpoint.
 *
 * @param endpoint The endpoint to connect to.
 * @param parameters The parameters to use for the connection.
 * @return The new connection object.
 */
- (nw_connection_t)createConnectionWithEndpoint:(nw_endpoint_t)endpoint
                                     parameters:(nw_parameters_t)parameters;

/**
 * Sets the dispatch queue for the connection.
 *
 * @param queue The dispatch queue to use for the connection.
 */
- (void)setQueue:(dispatch_queue_t)queue;

/**
 * Sets the state changed handler for the connection.
 *
 * @param handler The state changed handler to use for the connection.
 */
- (void)setStateChangedHandler:(nullable nw_connection_state_changed_handler_t)handler;

/**
 * Starts the connection.
 */
- (void)start;

/**
 * Cancels the connection.
 */
- (void)cancel;

@optional
/**
 * Initializes the connection object with a Network.framework connection object.
 *
 * @param connection The underlying nw_connection_t object.
 */
- (instancetype)initWithNWConnection:(nw_connection_t)connection;

@required
/**
 * Receives a message from the connection.
 *
 * @param minIncompleteLength The minimum number of bytes to receive before the completion handler
 * is called.
 * @param maxLengths The maximum number of bytes to receive.
 * @param handler The completion handler to call when the receive is complete or an error occurs.
 */
- (void)receiveMessageWithMinLength:(uint32_t)minIncompleteLength
                          maxLength:(uint32_t)maxLengths
                  completionHandler:(void (^)(dispatch_data_t _Nullable content,
                                              nw_content_context_t _Nullable context,
                                              bool isComplete, nw_error_t _Nullable error))handler;

/**
 * Sends data over the connection.
 *
 * @param content The data to send.
 * @param context The content context to use for the send.
 * @param isComplete A boolean indicating if this is the complete message.
 * @param handler The completion handler to call when the send is complete or an error occurs.
 */
- (void)sendData:(dispatch_data_t)content
              context:(nw_content_context_t)context
           isComplete:(BOOL)isComplete
    completionHandler:(void (^)(nw_error_t _Nullable error))handler;

/**
 * Returns the underlying nw_connection_t object.
 *
 * @return The underlying nw_connection_t object, or nil if not applicable.
 */
- (nullable nw_connection_t)nwConnection;

@end

NS_ASSUME_NONNULL_END
